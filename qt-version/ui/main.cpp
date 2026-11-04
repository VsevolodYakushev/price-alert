#include <QApplication>
#include <QFile>
#include <QFont>
#include <QPalette>
#include <QStyleFactory>

#include "mock/MockAuthService.h"
#include "mock/MockPortfolioService.h"
#include "ui/AlertBridge.h"
#include "ui/AuthWindow.h"
#include "ui/Format.h"
#include "ui/MainWindow.h"
#include "ui/TrayNotifier.h"

namespace {

void applyTheme(QApplication& app) {
    app.setStyle(QStyleFactory::create("Fusion"));

    // Палитра нужна для того, что не покрывает style.qss: диалоги, меню, стрелки, подсказки.
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#0F1115"));
    palette.setColor(QPalette::WindowText, QColor("#E6E8EB"));
    palette.setColor(QPalette::Base, QColor("#171A21"));
    palette.setColor(QPalette::AlternateBase, QColor("#1B1F27"));
    palette.setColor(QPalette::Text, QColor("#E6E8EB"));
    palette.setColor(QPalette::Button, QColor("#222733"));
    palette.setColor(QPalette::ButtonText, QColor("#E6E8EB"));
    palette.setColor(QPalette::Highlight, QColor("#3B82F6"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::ToolTipBase, QColor("#171A21"));
    palette.setColor(QPalette::ToolTipText, QColor("#E6E8EB"));
    palette.setColor(QPalette::PlaceholderText, QColor("#5B6270"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#5B6270"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#5B6270"));
    app.setPalette(palette);
    app.setFont(QFont("Segoe UI", 10));

    QFile qss(":/style.qss");
    if (qss.open(QIODevice::ReadOnly))
        app.setStyleSheet(QString::fromUtf8(qss.readAll()));
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setApplicationName("Price Alert");
    // Закрытое окно уходит в трей, приложение продолжает следить за ценами.
    QApplication::setQuitOnLastWindowClosed(false);
    applyTheme(app);
    QApplication::setWindowIcon(TrayNotifier::appIcon());

    // Мост создаётся раньше сервисов и уничтожается позже: поток ядра обращается к нему до самого конца.
    AlertBridge bridge;

    // Мок-ядро. Когда Студент 2 сдаст свою часть, здесь подставляются реальные AuthService и PortfolioService.
    MockAuthService auth;
    MockPortfolioService portfolio;
    portfolio.onAlert([&bridge](const Alert& alert) { bridge.post(alert); });

    TrayNotifier tray;
    AuthWindow authWindow(auth);
    authWindow.setHint("Демо-вход: demo / demo123, код подтверждения 123456");
    MainWindow mainWindow(auth, portfolio);
    mainWindow.setCloseToTray(tray.isAvailable());

    QObject::connect(&authWindow, &AuthWindow::authenticated, [&] {
        authWindow.hide();
        mainWindow.startSession();
        mainWindow.show();
        mainWindow.raise();
        mainWindow.activateWindow();
    });

    QObject::connect(&mainWindow, &MainWindow::logoutRequested, [&] {
        auth.logout();
        mainWindow.stopSession();
        mainWindow.hide();
        authWindow.reset();
        authWindow.show();
    });

    QObject::connect(&bridge, &AlertBridge::alertReceived, [&](const Alert& alert) {
        if (!auth.currentUser())
            return;
        mainWindow.addAlertToLog(alert);
        tray.notify(QString("%1: цена достигла цели").arg(Format::qs(alert.symbol)),
                    QString("Цена %1 ≤ цель %2").arg(Format::price(alert.price), Format::price(alert.threshold)));
    });

    bool trayHintShown = false;
    QObject::connect(&mainWindow, &MainWindow::minimizedToTray, [&] {
        if (trayHintShown)
            return;
        trayHintShown = true;
        tray.notify("Price Alert работает в трее", "Уведомления о ценах продолжат приходить. Выход — через меню иконки.");
    });

    QObject::connect(&tray, &TrayNotifier::openRequested, [&] {
        QWidget* window = auth.currentUser() ? static_cast<QWidget*>(&mainWindow) : &authWindow;
        window->showNormal();
        window->raise();
        window->activateWindow();
    });
    QObject::connect(&tray, &TrayNotifier::quitRequested, &app, &QApplication::quit);

    authWindow.show();
    const int rc = app.exec();

    // Отписываемся до разрушения окон: после этого поток ядра алерты больше не шлёт.
    portfolio.onAlert(nullptr);
    return rc;
}
