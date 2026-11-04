#pragma once

#include <map>
#include <optional>
#include <string>

#include "core_api/AuthService.h"

// Заглушка ядра для разработки UI: пользователи в памяти, код подтверждения всегда 123456.
// Встроенный пользователь: demo / demo123.
class MockAuthService : public AuthService {
public:
    MockAuthService();

    AuthResult registerUser(const std::string& username, const std::string& email,
                            const std::string& password) override;
    AuthResult login(const std::string& username, const std::string& password) override;
    AuthResult verifyCode(const std::string& username, const std::string& code) override;
    void logout() override;
    std::optional<User> currentUser() const override;

private:
    struct Account {
        User user;
        std::string password;
    };

    std::map<std::string, Account> accounts_;
    std::optional<std::string> pendingUser_;  // прошёл пароль, ждёт код
    std::optional<User> current_;
    int64_t nextId_ = 1;
};
