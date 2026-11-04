#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "core_api/Types.h"

using AlertCallback = std::function<void(const Alert&)>;

// Портфель текущего пользователя.
// addAsset / removeAsset / setThreshold вызываются из UI-потока.
// snapshot() безопасно вызывать откуда угодно: возвращает копию.
// Колбэк onAlert вызывается из потока ядра, НЕ из главного: UI сам перекидывает его в главный поток.
class PortfolioService {
public:
    virtual ~PortfolioService() = default;

    // Возвращает id нового актива или nullopt, если добавить нельзя (дубль, неизвестный тикер).
    virtual std::optional<int64_t> addAsset(const std::string& symbol, double target_price) = 0;
    virtual void removeAsset(int64_t id) = 0;
    virtual void setThreshold(int64_t id, double price) = 0;
    virtual std::vector<AssetSnapshot> snapshot() const = 0;
    // nullptr отписывает; после возврата из onAlert(nullptr) старый колбэк больше не вызывается.
    virtual void onAlert(AlertCallback callback) = 0;
};
