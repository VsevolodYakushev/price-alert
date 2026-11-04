#include "ui/TrayNotifier.h"

#include <QAction>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QSystemTrayIcon>

TrayNotifier::TrayNotifier(QObject* parent)
    : QObject(parent), menu_(std::make_unique<QMenu>()), tray_(new QSystemTrayIcon(appIcon(), this)) {
    auto* open = menu_->addAction("Открыть");
    menu_->addSeparator();
    auto* quit = menu_->addAction("Выход");
    connect(open, &QAction::triggered, this, &TrayNotifier::openRequested);
    connect(quit, &QAction::triggered, this, &TrayNotifier::quitRequested);

    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
            emit openRequested();
    });
    connect(tray_, &QSystemTrayIcon::messageClicked, this, &TrayNotifier::openRequested);

    tray_->setContextMenu(menu_.get());
    tray_->setToolTip("Price Alert");
    tray_->show();
}

TrayNotifier::~TrayNotifier() {
    // Трей удаляем раньше меню, на которое он ссылается.
    delete tray_;
}

bool TrayNotifier::isAvailable() const {
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayNotifier::notify(const QString& title, const QString& text) {
    if (tray_->isVisible())
        tray_->showMessage(title, text, appIcon(), 6000);
}

QIcon TrayNotifier::appIcon() {
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#3B82F6"));
    painter.drawRoundedRect(QRectF(2, 2, 60, 60), 14, 14);

    painter.setPen(QPen(Qt::white, 6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    const QPointF line[] = {{12, 20}, {26, 34}, {36, 26}, {52, 46}};
    painter.drawPolyline(line, 4);
    painter.end();

    return QIcon(pixmap);
}
