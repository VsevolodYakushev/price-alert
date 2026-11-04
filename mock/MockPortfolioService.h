#pragma once

#include <condition_variable>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "core_api/PortfolioService.h"

// Заглушка ядра для разработки UI. Фоновый поток раз в 500 мс двигает цены случайным
// блужданием и шлёт алерт, когда цена опускается до цели (повторно — только после отката выше).
// Колбэк вызывается из фонового потока, как и у настоящего ядра.
class MockPortfolioService : public PortfolioService {
public:
    MockPortfolioService();
    ~MockPortfolioService() override;

    std::optional<int64_t> addAsset(const std::string& symbol, double target_price) override;
    void removeAsset(int64_t id) override;
    void setThreshold(int64_t id, double price) override;
    std::vector<AssetSnapshot> snapshot() const override;
    void onAlert(AlertCallback callback) override;

    // Только для демо: заполняет портфель тестовыми тикерами.
    void seedDemoData();

private:
    struct Entry {
        AssetSnapshot snap;
        double startPrice = 0.0;
        bool armed = true;
    };

    void run();
    std::vector<Alert> tickLocked();
    double startPriceFor(const std::string& symbol);

    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::vector<Entry> entries_;
    int64_t nextId_ = 1;
    std::mt19937 rng_{std::random_device{}()};
    bool stop_ = false;

    std::mutex callbackMutex_;
    AlertCallback callback_;

    std::thread worker_;
};
