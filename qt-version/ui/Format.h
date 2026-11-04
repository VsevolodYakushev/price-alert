#pragma once

#include <QDateTime>
#include <QFont>
#include <QLabel>
#include <QLocale>
#include <QString>
#include <QStyle>

#include <chrono>
#include <string>

// Мелкие помощники UI: форматирование чисел и времени, сообщения под формами.
namespace Format {

inline QString qs(const std::string& s) {
    return QString::fromStdString(s);
}

inline QString price(double p) {
    static const QLocale ru(QLocale::Russian);
    return ru.toString(p, 'f', p >= 1.0 ? 2 : 6);
}

inline QString percent(double p) {
    static const QLocale ru(QLocale::Russian);
    return (p > 0 ? "+" : "") + ru.toString(p, 'f', 2) + " %";
}

inline qint64 toMs(std::chrono::system_clock::time_point ts) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(ts.time_since_epoch()).count();
}

inline QString time(std::chrono::system_clock::time_point ts) {
    return QDateTime::fromMSecsSinceEpoch(toMs(ts)).toString("hh:mm:ss");
}

inline QFont monoFont() {
    QFont font("Consolas");
    font.setStyleHint(QFont::Monospace);
    return font;
}

// Цвет сообщения задаётся в style.qss по свойству kind (error / info).
inline void setFormMessage(QLabel* label, const QString& text, bool error) {
    label->setProperty("kind", error ? "error" : "info");
    label->style()->unpolish(label);
    label->style()->polish(label);
    label->setText(text);
}

}  // namespace Format
