#include "ui/AuthWindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "ui/AuthMessages.h"
#include "ui/Format.h"

namespace {

QLineEdit* makeField(const QString& placeholder, bool password = false) {
    auto* edit = new QLineEdit;
    edit->setPlaceholderText(placeholder);
    if (password)
        edit->setEchoMode(QLineEdit::Password);
    return edit;
}

QLabel* makeMessageLabel() {
    auto* label = new QLabel;
    label->setObjectName("formMessage");
    label->setWordWrap(true);
    return label;
}

QPushButton* makeButton(const QString& text, const char* objectName) {
    auto* button = new QPushButton(text);
    button->setObjectName(objectName);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}  // namespace

AuthWindow::AuthWindow(AuthService& auth, QWidget* parent) : QWidget(parent), auth_(auth) {
    setWindowTitle("Price Alert — вход");
    resize(480, 600);

    auto* card = new QFrame;
    card->setObjectName("card");
    card->setFixedWidth(380);

    auto* title = new QLabel("Price Alert");
    title->setObjectName("appTitle");
    auto* subtitle = new QLabel("Уведомления о ценах на Binance");
    subtitle->setObjectName("muted");
    hint_ = new QLabel;
    hint_->setObjectName("hint");
    hint_->setWordWrap(true);
    hint_->hide();

    loginPage_ = buildLoginPage();
    registerPage_ = buildRegisterPage();
    codePage_ = buildCodePage();
    stack_ = new QStackedWidget;
    stack_->addWidget(loginPage_);
    stack_->addWidget(registerPage_);
    stack_->addWidget(codePage_);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(28, 28, 28, 28);
    cardLayout->setSpacing(6);
    cardLayout->addWidget(title);
    cardLayout->addWidget(subtitle);
    cardLayout->addWidget(hint_);
    cardLayout->addSpacing(16);
    cardLayout->addWidget(stack_);

    auto* root = new QVBoxLayout(this);
    root->addStretch();
    root->addWidget(card, 0, Qt::AlignHCenter);
    root->addStretch();
}

QWidget* AuthWindow::buildLoginPage() {
    auto* page = new QWidget;
    loginUser_ = makeField("Имя пользователя");
    loginPass_ = makeField("Пароль", true);
    loginMessage_ = makeMessageLabel();
    auto* submit = makeButton("Войти", "primary");
    auto* toRegister = makeButton("Нет аккаунта? Зарегистрироваться", "link");

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(loginUser_);
    layout->addWidget(loginPass_);
    layout->addWidget(loginMessage_);
    layout->addWidget(submit);
    layout->addWidget(toRegister, 0, Qt::AlignHCenter);
    layout->addStretch();

    connect(submit, &QPushButton::clicked, this, &AuthWindow::submitLogin);
    connect(loginUser_, &QLineEdit::returnPressed, this, &AuthWindow::submitLogin);
    connect(loginPass_, &QLineEdit::returnPressed, this, &AuthWindow::submitLogin);
    connect(toRegister, &QPushButton::clicked, this, [this] { showPage(registerPage_); });
    return page;
}

QWidget* AuthWindow::buildRegisterPage() {
    auto* page = new QWidget;
    regUser_ = makeField("Имя пользователя");
    regEmail_ = makeField("Email");
    regPass_ = makeField("Пароль, не короче 6 символов", true);
    regPass2_ = makeField("Повторите пароль", true);
    regMessage_ = makeMessageLabel();
    auto* submit = makeButton("Создать аккаунт", "primary");
    auto* toLogin = makeButton("Уже есть аккаунт? Войти", "link");

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(regUser_);
    layout->addWidget(regEmail_);
    layout->addWidget(regPass_);
    layout->addWidget(regPass2_);
    layout->addWidget(regMessage_);
    layout->addWidget(submit);
    layout->addWidget(toLogin, 0, Qt::AlignHCenter);
    layout->addStretch();

    connect(submit, &QPushButton::clicked, this, &AuthWindow::submitRegister);
    connect(regPass2_, &QLineEdit::returnPressed, this, &AuthWindow::submitRegister);
    connect(toLogin, &QPushButton::clicked, this, [this] { showPage(loginPage_); });
    return page;
}

QWidget* AuthWindow::buildCodePage() {
    auto* page = new QWidget;
    codeInfo_ = new QLabel;
    codeInfo_->setObjectName("muted");
    codeInfo_->setWordWrap(true);
    code_ = makeField("Код из письма");
    code_->setMaxLength(8);
    code_->setValidator(new QRegularExpressionValidator(QRegularExpression("\\d{0,8}"), code_));
    codeMessage_ = makeMessageLabel();
    auto* submit = makeButton("Подтвердить", "primary");
    auto* back = makeButton("Назад ко входу", "link");

    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);
    layout->addWidget(codeInfo_);
    layout->addWidget(code_);
    layout->addWidget(codeMessage_);
    layout->addWidget(submit);
    layout->addWidget(back, 0, Qt::AlignHCenter);
    layout->addStretch();

    connect(submit, &QPushButton::clicked, this, &AuthWindow::submitCode);
    connect(code_, &QLineEdit::returnPressed, this, &AuthWindow::submitCode);
    connect(back, &QPushButton::clicked, this, [this] { showPage(loginPage_); });
    return page;
}

void AuthWindow::setHint(const QString& text) {
    hint_->setText(text);
    hint_->setVisible(!text.isEmpty());
}

void AuthWindow::reset() {
    for (auto* edit : {loginUser_, loginPass_, regUser_, regEmail_, regPass_, regPass2_, code_})
        edit->clear();
    for (auto* label : {loginMessage_, regMessage_, codeMessage_})
        label->clear();
    pendingUser_.clear();
    showPage(loginPage_);
}

void AuthWindow::closeEvent(QCloseEvent* event) {
    // Окно входа видно только без активной сессии: закрыть его значит выйти из приложения.
    event->accept();
    QCoreApplication::quit();
}

void AuthWindow::showPage(QWidget* page) {
    stack_->setCurrentWidget(page);
    if (page == loginPage_)
        loginUser_->setFocus();
    else if (page == registerPage_)
        regUser_->setFocus();
    else
        code_->setFocus();
}

void AuthWindow::submitLogin() {
    const QString username = loginUser_->text().trimmed();
    if (username.isEmpty() || loginPass_->text().isEmpty()) {
        Format::setFormMessage(loginMessage_, "Введите имя пользователя и пароль", true);
        return;
    }
    const auto result = auth_.login(username.toStdString(), loginPass_->text().toStdString());
    loginPass_->clear();
    handleResult(result, username, loginMessage_);
}

void AuthWindow::submitRegister() {
    static const QRegularExpression usernameRe("^[A-Za-z0-9_]{3,32}$");
    static const QRegularExpression emailRe("^[^@\\s]+@[^@\\s]+\\.[^@\\s]+$");

    const QString username = regUser_->text().trimmed();
    const QString email = regEmail_->text().trimmed();
    const QString password = regPass_->text();

    QString error;
    if (username.isEmpty() || email.isEmpty() || password.isEmpty() || regPass2_->text().isEmpty())
        error = "Заполните все поля";
    else if (!usernameRe.match(username).hasMatch())
        error = "Имя пользователя: 3–32 символа, латиница, цифры и _";
    else if (!emailRe.match(email).hasMatch())
        error = "Некорректный email";
    else if (password.size() < 6)
        error = "Пароль должен быть не короче 6 символов";
    else if (password != regPass2_->text())
        error = "Пароли не совпадают";
    if (!error.isEmpty()) {
        Format::setFormMessage(regMessage_, error, true);
        return;
    }

    const auto result = auth_.registerUser(username.toStdString(), email.toStdString(), password.toStdString());
    handleResult(result, username, regMessage_);
    if (result == AuthResult::Ok || result == AuthResult::NeedCode) {
        regPass_->clear();
        regPass2_->clear();
    }
}

void AuthWindow::submitCode() {
    if (code_->text().isEmpty()) {
        Format::setFormMessage(codeMessage_, "Введите код из письма", true);
        return;
    }
    const auto result = auth_.verifyCode(pendingUser_.toStdString(), code_->text().toStdString());
    handleResult(result, pendingUser_, codeMessage_);
}

void AuthWindow::handleResult(AuthResult result, const QString& username, QLabel* messageLabel) {
    switch (result) {
    case AuthResult::Ok:
        if (auth_.currentUser()) {
            reset();
            emit authenticated();
        } else {
            // Регистрация прошла без кода, но сессия не открыта: просим войти.
            showPage(loginPage_);
            loginUser_->setText(username);
            loginPass_->setFocus();
            Format::setFormMessage(loginMessage_, "Аккаунт создан. Войдите с новым паролем", false);
        }
        return;
    case AuthResult::NeedCode:
        pendingUser_ = username;
        code_->clear();
        codeMessage_->clear();
        codeInfo_->setText(QString("Мы отправили код на почту пользователя %1. Введите его ниже.").arg(username));
        showPage(codePage_);
        return;
    default:
        Format::setFormMessage(messageLabel, authMessage(result), true);
        return;
    }
}
