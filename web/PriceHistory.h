#pragma once

#include <chrono>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core_api/PortfolioService.h"

// История цен для графика. Ядро отдаёт только последнюю цену, поэтому UI-слой сам
// раз в секунду снимает snapshot() (его можно вызывать из любого потока) и копит точки.
// Так график заполнен сразу после входа и не теряется при перезагрузке страницы.
class PriceHistory {
public:
    using Point = std::pair<int64_t, double>;  // время (мс), цена

    explicit PriceHistory(const PortfolioService& portfolio) : portfolio_(portfolio), worker_([this] { run(); }) {}

    ~PriceHistory() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        wake_.notify_all();
        worker_.join();
    }

    std::vector<Point> get(int64_t id) const {
        std::lock_guard lock(mutex_);
        const auto it = points_.find(id);
        if (it == points_.end())
            return {};
        return {it->second.begin(), it->second.end()};
    }

private:
    static constexpr size_t kMaxPoints = 300;

    void run() {
        std::unique_lock lock(mutex_);
        while (!stop_) {
            lock.unlock();
            const auto rows = portfolio_.snapshot();
            lock.lock();
            sample(rows);
            wake_.wait_for(lock, std::chrono::seconds(1), [this] { return stop_; });
        }
    }

    void sample(const std::vector<AssetSnapshot>& rows) {
        std::unordered_set<int64_t> alive;
        for (const auto& s : rows) {
            alive.insert(s.asset.id);
            if (!s.last_price)
                continue;
            const int64_t ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(s.ts.time_since_epoch()).count();
            auto& points = points_[s.asset.id];
            if (!points.empty() && points.back().first >= ms)
                continue;
            points.emplace_back(ms, *s.last_price);
            if (points.size() > kMaxPoints)
                points.pop_front();
        }
        for (auto it = points_.begin(); it != points_.end();)
            it = alive.count(it->first) ? std::next(it) : points_.erase(it);
    }

    const PortfolioService& portfolio_;
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::unordered_map<int64_t, std::deque<Point>> points_;
    bool stop_ = false;
    std::thread worker_;
};
