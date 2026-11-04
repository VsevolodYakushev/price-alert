#include "ui/AlertBridge.h"

void AlertBridge::post(const Alert& alert) {
    // Лямбда хранит копию алерта и выполнится в потоке, которому принадлежит мост, то есть в главном.
    QMetaObject::invokeMethod(this, [this, alert] { emit alertReceived(alert); }, Qt::QueuedConnection);
}
