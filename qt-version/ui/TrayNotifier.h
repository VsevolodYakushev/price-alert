#pragma once

#include <QIcon>
#include <QObject>

#include <memory>

class QMenu;
class QSystemTrayIcon;

// Иконка в трее и всплывающие системные уведомления.
class TrayNotifier : public QObject {
    Q_OBJECT
public:
    explicit TrayNotifier(QObject* parent = nullptr);
    ~TrayNotifier() override;

    bool isAvailable() const;
    void notify(const QString& title, const QString& text);

    // Иконка приложения рисуется кодом, чтобы не возить файлы ресурсов.
    static QIcon appIcon();

signals:
    void openRequested();
    void quitRequested();

private:
    std::unique_ptr<QMenu> menu_;
    QSystemTrayIcon* tray_;
};
