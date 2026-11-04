#include "mock/MockAuthService.h"

namespace {
const char* const kValidCode = "123456";
}

MockAuthService::MockAuthService() {
    registerUser("demo", "demo@example.com", "demo123");
    pendingUser_.reset();
}

AuthResult MockAuthService::registerUser(const std::string& username, const std::string& email,
                                         const std::string& password) {
    if (username.empty() || email.empty() || password.empty())
        return AuthResult::InvalidInput;
    if (accounts_.count(username))
        return AuthResult::UserExists;
    for (const auto& [name, account] : accounts_)
        if (account.user.email == email)
            return AuthResult::UserExists;

    accounts_[username] = Account{User{nextId_++, username, email}, password};
    pendingUser_ = username;
    return AuthResult::NeedCode;
}

AuthResult MockAuthService::login(const std::string& username, const std::string& password) {
    const auto it = accounts_.find(username);
    if (it == accounts_.end() || it->second.password != password)
        return AuthResult::BadCredentials;

    pendingUser_ = username;
    return AuthResult::NeedCode;
}

AuthResult MockAuthService::verifyCode(const std::string& username, const std::string& code) {
    if (!pendingUser_ || *pendingUser_ != username)
        return AuthResult::BadCredentials;
    if (code != kValidCode)
        return AuthResult::BadCode;

    current_ = accounts_.at(username).user;
    pendingUser_.reset();
    return AuthResult::Ok;
}

void MockAuthService::logout() {
    current_.reset();
    pendingUser_.reset();
}

std::optional<User> MockAuthService::currentUser() const {
    return current_;
}
