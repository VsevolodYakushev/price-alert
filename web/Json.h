#pragma once

#include <chrono>
#include <cstdio>
#include <optional>
#include <string>

#include "core_api/Types.h"

// Минимальная сборка JSON-ответов: наружу уходят только эти несколько структур.
namespace json {

inline std::string str(const std::string& s) {
    std::string out = "\"";
    for (const unsigned char c : s) {
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            } else {
                out += static_cast<char>(c);
            }
        }
    }
    return out + "\"";
}

inline std::string num(double v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.10g", v);
    return buf;
}

inline std::string num(int64_t v) {
    return std::to_string(v);
}

inline std::string num(uint64_t v) {
    return std::to_string(v);
}

inline std::string ms(std::chrono::system_clock::time_point ts) {
    return num(static_cast<int64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count()));
}

inline std::string authResult(AuthResult r) {
    switch (r) {
    case AuthResult::Ok: return str("Ok");
    case AuthResult::NeedCode: return str("NeedCode");
    case AuthResult::BadCredentials: return str("BadCredentials");
    case AuthResult::UserExists: return str("UserExists");
    case AuthResult::BadCode: return str("BadCode");
    case AuthResult::InvalidInput: return str("InvalidInput");
    case AuthResult::Error: break;
    }
    return str("Error");
}

inline std::string user(const std::optional<User>& u) {
    if (!u)
        return "null";
    return "{\"id\":" + num(u->id) + ",\"username\":" + str(u->username) + ",\"email\":" + str(u->email) + "}";
}

inline std::string asset(const AssetSnapshot& s) {
    return "{\"id\":" + num(s.asset.id) + ",\"symbol\":" + str(s.asset.symbol) +
           ",\"target\":" + num(s.asset.target_price) + ",\"enabled\":" + (s.asset.enabled ? "true" : "false") +
           ",\"price\":" + (s.last_price ? num(*s.last_price) : "null") + ",\"ts\":" + ms(s.ts) + "}";
}

inline std::string alert(uint64_t seq, const Alert& a) {
    return "{\"seq\":" + num(seq) + ",\"symbol\":" + str(a.symbol) + ",\"price\":" + num(a.price) +
           ",\"threshold\":" + num(a.threshold) + ",\"ts\":" + ms(a.ts) + "}";
}

}  // namespace json
