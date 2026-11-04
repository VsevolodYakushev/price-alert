#include "ui/AuthMessages.h"

QString authMessage(AuthResult result) {
    switch (result) {
    case AuthResult::Ok:
        return {};
    case AuthResult::NeedCode:
        return "Введите код подтверждения из письма";
    case AuthResult::BadCredentials:
        return "Неверное имя пользователя или пароль";
    case AuthResult::UserExists:
        return "Пользователь с таким именем или email уже существует";
    case AuthResult::BadCode:
        return "Неверный код подтверждения";
    case AuthResult::InvalidInput:
        return "Проверьте введённые данные";
    case AuthResult::Error:
        break;
    }
    return "Что-то пошло не так. Попробуйте ещё раз";
}
