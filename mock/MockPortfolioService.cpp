#include "mock/MockPortfolioService.h"

#include <algorithm>
#include <chrono>
#include <map>

using namespace std::chrono_literals;

MockPortfolioService::MockPortfolioService() : worker_([this] { run(); }) {}

MockPortfolioService::~MockPortfolioService() {
    {
        std::lock_guard lock(mutex_);
        stop_ = true;
    }
    wake_.notify_all();
    worker_.join();
}

std::optional<int64_t> MockPortfolioService::addAsset(const std::string& symbol, double target_price) {
    std::lock_guard lock(mutex_);
    const bool exists = std::any_of(entries_.begin(), entries_.end(),
                                    [&](const Entry& e) { return e.snap.asset.symbol == symbol; });
    if (exists || symbol.empty() || target_price <= 0.0)
        return std::nullopt;

    Entry entry;
    entry.snap.asset = AssetConfig{nextId_++, symbol, target_price, true};
    entry.startPrice = startPriceFor(symbol);
    entries_.push_back(entry);
    return entry.snap.asset.id;
}

void MockPortfolioService::removeAsset(int64_t id) {
    std::lock_guard lock(mutex_);
    entries_.erase(std::remove_if(entries_.begin(), entries_.end(),
                                  [id](const Entry& e) { return e.snap.asset.id == id; }),
                   entries_.end());
}

void MockPortfolioService::setThreshold(int64_t id, double price) {
    std::lock_guard lock(mutex_);
    for (auto& e : entries_) {
        if (e.snap.asset.id == id) {
            e.snap.asset.target_price = price;
            e.armed = true;
        }
    }
}

std::vector<AssetSnapshot> MockPortfolioService::snapshot() const {
    std::lock_guard lock(mutex_);
    std::vector<AssetSnapshot> result;
    result.reserve(entries_.size());
    for (const auto& e : entries_)
        result.push_back(e.snap);
    return result;
}

void MockPortfolioService::onAlert(AlertCallback callback) {
    std::lock_guard lock(callbackMutex_);
    callback_ = std::move(callback);
}

void MockPortfolioService::seedDemoData() {
    struct Demo {
        const char* symbol;
        double target;
        bool enabled;
    };
    // Цели рядом с ценой, чтобы алерты срабатывали в первые минуты; SOL и DOGE уже ниже цели.
    static const Demo demo[] = {
        {"BTCUSDT", 67850.0, true}, {"ETHUSDT", 3492.0, true}, {"SOLUSDT", 160.3, true},
        {"BNBUSDT", 585.0, true},   {"XRPUSDT", 0.598, true},  {"DOGEUSDT", 0.1503, true},
        {"ADAUSDT", 0.44, false},
    };
    for (const auto& d : demo) {
        const auto id = addAsset(d.symbol, d.target);
        if (id && !d.enabled) {
            std::lock_guard lock(mutex_);
            for (auto& e : entries_)
                if (e.snap.asset.id == *id)
                    e.snap.asset.enabled = false;
        }
    }
}

void MockPortfolioService::run() {
    std::unique_lock lock(mutex_);
    while (!wake_.wait_for(lock, 500ms, [this] { return stop_; })) {
        const auto fired = tickLocked();
        lock.unlock();
        {
            std::lock_guard cbLock(callbackMutex_);
            if (callback_)
                for (const auto& alert : fired)
                    callback_(alert);
        }
        lock.lock();
    }
}

std::vector<Alert> MockPortfolioService::tickLocked() {
    std::normal_distribution<double> step(0.0, 0.002);
    const auto now = std::chrono::system_clock::now();
    std::vector<Alert> fired;

    for (auto& e : entries_) {
        auto& snap = e.snap;
        snap.last_price = snap.last_price ? *snap.last_price * (1.0 + step(rng_)) : e.startPrice;
        snap.ts = now;

        if (!snap.asset.enabled)
            continue;
        const double price = *snap.last_price;
        if (e.armed && price <= snap.asset.target_price) {
            fired.push_back(Alert{snap.asset.symbol, price, snap.asset.target_price, now});
            e.armed = false;
        } else if (!e.armed && price > snap.asset.target_price) {
            e.armed = true;
        }
    }
    return fired;
}

double MockPortfolioService::startPriceFor(const std::string& symbol) {
    static const std::map<std::string, double> known = {
        {"BTCUSDT", 68000.0}, {"ETHUSDT", 3500.0}, {"BNBUSDT", 600.0},
        {"SOLUSDT", 160.0},   {"XRPUSDT", 0.6},    {"DOGEUSDT", 0.15},
        {"ADAUSDT", 0.45},
    };
    const auto it = known.find(symbol);
    if (it != known.end())
        return it->second;
    return std::uniform_real_distribution<double>(1.0, 200.0)(rng_);
}
