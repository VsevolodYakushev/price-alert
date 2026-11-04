#pragma once

#include <optional>
#include <string>

#include "core_api/Types.h"

// Вход и регистрация. Все методы вызываются из UI-потока.
class AuthService {
public:
    virtual ~AuthService() = default;

    virtual AuthResult registerUser(const std::string& username, const std::string& email,
                                    const std::string& password) = 0;
    virtual AuthResult login(const std::string& username, const std::string& password) = 0;
    virtual AuthResult verifyCode(const std::string& username, const std::string& code) = 0;
    virtual void logout() = 0;
    virtual std::optional<User> currentUser() const = 0;
};
