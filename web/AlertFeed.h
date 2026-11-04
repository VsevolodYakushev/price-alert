#pragma once

#include <cstdint>
#include <deque>
#include <mutex>
#include <utility>
#include <vector>

#include "core_api/Types.h"

// Потокобезопасная лента алертов. Ядро кладёт в неё алерты из своего потока,
// HTTP-обработчики забирают новые по номеру курсора. Каждому алерту присваивается
// возрастающий номер, поэтому браузер получает каждый алерт ровно один раз.
class AlertFeed {
public:
    using Item = std::pair<uint64_t, Alert>;

    // Вызывается из потока ядра.
    void push(const Alert& alert) {
        std::lock_guard lock(mutex_);
        items_.emplace_back(++last_, alert);
        if (items_.size() > kMaxItems)
            items_.pop_front();
    }

    // Алерты с номером больше after.
    std::vector<Item> since(uint64_t after) const {
        std::lock_guard lock(mutex_);
        std::vector<Item> result;
        for (const auto& item : items_)
            if (item.first > after)
                result.push_back(item);
        return result;
    }

    uint64_t last() const {
        std::lock_guard lock(mutex_);
        return last_;
    }

private:
    static constexpr size_t kMaxItems = 200;

    mutable std::mutex mutex_;
    std::deque<Item> items_;
    uint64_t last_ = 0;
};
