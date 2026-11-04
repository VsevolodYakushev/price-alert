#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

// Общие структуры данных между ядром (Студент 2) и UI (Студент 3).

struct User {
    int64_t id = 0;
    std::string username;
    std::string email;
};

// Отслеживаемый актив.
struct AssetConfig {
    int64_t id = 0;
    std::string symbol;  // как у Binance, заглавными: BTCUSDT
    double target_price = 0.0;
    bool enabled = true;
};

// Строка таблицы на дашборде: актив + последняя цена.
struct AssetSnapshot {
    AssetConfig asset;
    std::optional<double> last_price;            // пусто, пока котировка не пришла
    std::chrono::system_clock::time_point ts{};  // время последней котировки
};

// Сработавший сигнал.
struct Alert {
    std::string symbol;
    double price = 0.0;
    double threshold = 0.0;
    std::chrono::system_clock::time_point ts{};
};

// Результат входа/регистрации. Тексты ошибок для пользователя пишет UI.
enum class AuthResult {
    Ok,
    NeedCode,        // нужен код из почты (этап 2)
    BadCredentials,
    UserExists,
    BadCode,
    InvalidInput,
    Error,
};
