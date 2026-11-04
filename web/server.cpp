// Локальный веб-интерфейс Price Alert: C++-сервер на 127.0.0.1, UI — страница в браузере.
//
// Потоки:
//  * cpp-httplib обрабатывает запросы в пуле потоков. Ядро требует, чтобы методы
//    вызывались из одного «UI-потока», поэтому все вызовы сервисов (кроме snapshot())
//    идут под одним мьютексом uiMutex — по одному за раз, как из главного потока.
//  * snapshot() ядро разрешает вызывать откуда угодно, его не блокируем.
//  * Колбэк onAlert приходит из потока ядра и только кладёт алерт в AlertFeed
//    (потокобезопасная лента). Браузер забирает новые алерты при опросе /api/state.

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

#include "third_party/httplib.h"

#ifdef _WIN32
#include <shellapi.h>
#endif

#include "mock/MockAuthService.h"
#include "mock/MockPortfolioService.h"
#include "web/AlertFeed.h"
#include "web/Json.h"
#include "web/PriceHistory.h"

namespace {

constexpr int kDefaultPort = 8080;

httplib::Server* g_server = nullptr;

void sendJson(httplib::Response& res, const std::string& body, int status = 200) {
    res.status = status;
    res.set_header("Cache-Control", "no-store");
    res.set_content(body, "application/json; charset=utf-8");
}

void sendError(httplib::Response& res, int status, const std::string& code) {
    sendJson(res, "{\"error\":" + json::str(code) + "}", status);
}

std::optional<int64_t> paramId(const httplib::Request& req, const char* name) {
    try {
        return std::stoll(req.get_param_value(name));
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> paramPrice(const httplib::Request& req, const char* name) {
    std::string text = req.get_param_value(name);
    for (auto& c : text)
        if (c == ',')
            c = '.';
    char* end = nullptr;
    const double value = std::strtod(text.c_str(), &end);
    if (text.empty() || *end != '\0' || !(value > 0.0))
        return std::nullopt;
    return value;
}

}  // namespace

// Запуск: PriceAlert.exe [порт] [--no-browser]
int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);  // русский текст в консоли
#endif
    int port = kDefaultPort;
    bool openBrowser = true;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--no-browser")
            openBrowser = false;
        else if (const int value = std::atoi(arg.c_str()); value > 0 && value < 65536)
            port = value;
    }

    // Лента создаётся раньше сервисов и уничтожается позже: поток ядра пишет в неё до самого конца.
    AlertFeed feed;

    // Мок-ядро. Когда Студент 2 сдаст свою часть, здесь подставляются реальные AuthService и PortfolioService.
    MockAuthService auth;
    MockPortfolioService portfolio;
    portfolio.seedDemoData();  // тестовый портфель, чтобы было видно, как выглядит продукт
    portfolio.onAlert([&feed](const Alert& alert) { feed.push(alert); });

    // Объявлена после portfolio, значит уничтожится раньше него: её поток вызывает snapshot().
    PriceHistory history(portfolio);

    std::mutex uiMutex;
    httplib::Server server;
    g_server = &server;

    if (!server.set_mount_point("/", "web/static")) {
        std::cerr << "Не найдена папка web/static. Запускайте run.bat из папки проекта.\n";
        return 1;
    }

    // --- Вход и регистрация -------------------------------------------------

    server.Get("/api/me", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard lock(uiMutex);
        sendJson(res, "{\"user\":" + json::user(auth.currentUser()) + "}");
    });

    server.Post("/api/register", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard lock(uiMutex);
        const auto result = auth.registerUser(req.get_param_value("username"), req.get_param_value("email"),
                                              req.get_param_value("password"));
        sendJson(res, "{\"result\":" + json::authResult(result) + ",\"user\":" + json::user(auth.currentUser()) + "}");
    });

    server.Post("/api/login", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard lock(uiMutex);
        const auto result = auth.login(req.get_param_value("username"), req.get_param_value("password"));
        sendJson(res, "{\"result\":" + json::authResult(result) + ",\"user\":" + json::user(auth.currentUser()) + "}");
    });

    server.Post("/api/verify", [&](const httplib::Request& req, httplib::Response& res) {
        std::lock_guard lock(uiMutex);
        const auto result = auth.verifyCode(req.get_param_value("username"), req.get_param_value("code"));
        sendJson(res, "{\"result\":" + json::authResult(result) + ",\"user\":" + json::user(auth.currentUser()) + "}");
    });

    server.Post("/api/logout", [&](const httplib::Request&, httplib::Response& res) {
        std::lock_guard lock(uiMutex);
        auth.logout();
        sendJson(res, "{\"result\":\"Ok\"}");
    });

    // --- Портфель ------------------------------------------------------------

    auto loggedIn = [&] {
        std::lock_guard lock(uiMutex);
        return auth.currentUser().has_value();
    };

    // Состояние дашборда: таблица + алерты новее курсора after.
    // Без after возвращается только текущий курсор (чтобы не показывать старые алерты).
    server.Get("/api/state", [&](const httplib::Request& req, httplib::Response& res) {
        if (!loggedIn())
            return sendError(res, 401, "unauthorized");

        std::string body = "{\"assets\":[";
        const auto rows = portfolio.snapshot();
        for (size_t i = 0; i < rows.size(); ++i)
            body += (i ? "," : "") + json::asset(rows[i]);
        body += "],\"alerts\":[";

        uint64_t cursor = feed.last();
        if (req.has_param("after")) {
            uint64_t after = 0;
            try {
                after = std::stoull(req.get_param_value("after"));
            } catch (...) {
            }
            const auto items = feed.since(after);
            for (size_t i = 0; i < items.size(); ++i)
                body += (i ? "," : "") + json::alert(items[i].first, items[i].second);
            if (!items.empty())
                cursor = items.back().first;
            else
                cursor = std::max(after, cursor);
        }
        body += "],\"cursor\":" + json::num(cursor) + "}";
        sendJson(res, body);
    });

    // История цены одного актива для графика: [[мс, цена], ...].
    server.Get("/api/history", [&](const httplib::Request& req, httplib::Response& res) {
        if (!loggedIn())
            return sendError(res, 401, "unauthorized");
        const auto id = paramId(req, "id");
        if (!id)
            return sendError(res, 400, "bad_id");
        std::string body = "{\"points\":[";
        const auto points = history.get(*id);
        for (size_t i = 0; i < points.size(); ++i)
            body += (i ? ",[" : "[") + json::num(points[i].first) + "," + json::num(points[i].second) + "]";
        sendJson(res, body + "]}");
    });

    server.Post("/api/assets", [&](const httplib::Request& req, httplib::Response& res) {
        if (!loggedIn())
            return sendError(res, 401, "unauthorized");
        const auto target = paramPrice(req, "target");
        if (!target)
            return sendError(res, 400, "bad_target");
        std::lock_guard lock(uiMutex);
        const auto id = portfolio.addAsset(req.get_param_value("symbol"), *target);
        if (!id)
            return sendError(res, 409, "rejected");
        sendJson(res, "{\"id\":" + json::num(*id) + "}");
    });

    server.Post("/api/assets/remove", [&](const httplib::Request& req, httplib::Response& res) {
        if (!loggedIn())
            return sendError(res, 401, "unauthorized");
        const auto id = paramId(req, "id");
        if (!id)
            return sendError(res, 400, "bad_id");
        std::lock_guard lock(uiMutex);
        portfolio.removeAsset(*id);
        sendJson(res, "{\"result\":\"Ok\"}");
    });

    server.Post("/api/assets/threshold", [&](const httplib::Request& req, httplib::Response& res) {
        if (!loggedIn())
            return sendError(res, 401, "unauthorized");
        const auto id = paramId(req, "id");
        const auto target = paramPrice(req, "target");
        if (!id || !target)
            return sendError(res, 400, "bad_params");
        std::lock_guard lock(uiMutex);
        portfolio.setThreshold(*id, *target);
        sendJson(res, "{\"result\":\"Ok\"}");
    });

    // Ctrl+C в консоли — аккуратная остановка сервера.
    std::signal(SIGINT, [](int) {
        if (g_server)
            g_server->stop();
    });

    const std::string url = "http://localhost:" + std::to_string(port);
    if (!server.bind_to_port("127.0.0.1", port)) {
        std::cerr << "Порт " << port << " занят — возможно, Price Alert уже запущен. Откройте " << url << "\n";
        return 1;
    }
    std::cout << "Price Alert запущен: " << url << "\n"
              << "Демо-вход: demo / demo123, код 123456\n"
              << "Остановить: Ctrl+C или закрыть это окно\n";
#ifdef _WIN32
    if (openBrowser)
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#endif
    server.listen_after_bind();

    // Отписываемся до разрушения ленты: после этого поток ядра алерты больше не шлёт.
    portfolio.onAlert(nullptr);
    return 0;
}
