#pragma once

#include <QObject>

#include "core_api/Types.h"

Q_DECLARE_METATYPE(Alert)

// Принимает алерты из потока ядра и переизлучает их в главном (UI) потоке.
// Должен жить дольше PortfolioService, иначе поток ядра обратится к удалённому объекту.
class AlertBridge : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;

    // Потокобезопасно: можно вызывать из любого потока.
    void post(const Alert& alert);

signals:
    void alertReceived(const Alert& alert);
};
