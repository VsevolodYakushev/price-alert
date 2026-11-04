#pragma once

#include <QWidget>

#include "core_api/AuthService.h"

class QLabel;
class QLineEdit;
class QStackedWidget;

// Окно входа: три страницы — вход, регистрация, ввод кода из почты.
class AuthWindow : public QWidget {
    Q_OBJECT
public:
    explicit AuthWindow(AuthService& auth, QWidget* parent = nullptr);

    // Подсказка под заголовком (например, демо-логин для мок-ядра).
    void setHint(const QString& text);
    // Очищает поля и возвращает на страницу входа (после выхода из аккаунта).
    void reset();

signals:
    void authenticated();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    QWidget* buildLoginPage();
    QWidget* buildRegisterPage();
    QWidget* buildCodePage();

    void submitLogin();
    void submitRegister();
    void submitCode();
    void handleResult(AuthResult result, const QString& username, QLabel* messageLabel);
    void showPage(QWidget* page);

    AuthService& auth_;
    QStackedWidget* stack_ = nullptr;
    QLabel* hint_ = nullptr;

    QWidget* loginPage_ = nullptr;
    QLineEdit* loginUser_ = nullptr;
    QLineEdit* loginPass_ = nullptr;
    QLabel* loginMessage_ = nullptr;

    QWidget* registerPage_ = nullptr;
    QLineEdit* regUser_ = nullptr;
    QLineEdit* regEmail_ = nullptr;
    QLineEdit* regPass_ = nullptr;
    QLineEdit* regPass2_ = nullptr;
    QLabel* regMessage_ = nullptr;

    QWidget* codePage_ = nullptr;
    QLabel* codeInfo_ = nullptr;
    QLineEdit* code_ = nullptr;
    QLabel* codeMessage_ = nullptr;

    QString pendingUser_;
};
