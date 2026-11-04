#pragma once

#include <QChartView>
#include <QPointF>

#include <deque>
#include <unordered_map>
#include <vector>

#include "core_api/Types.h"

class QChart;
class QDateTimeAxis;
class QLineSeries;
class QValueAxis;

// График цены выбранного актива с линией цели. Историю копит сам UI из snapshot():
// ядро отдаёт только последнюю цену.
class PriceChart : public QChartView {
    Q_OBJECT
public:
    explicit PriceChart(QWidget* parent = nullptr);

    // Добавляет новые точки по всем активам; историю удалённых активов забывает.
    void appendHistory(const std::vector<AssetSnapshot>& rows);
    // Показывает актив или заглушку, если nullptr.
    void display(const AssetSnapshot* selected);
    void clear();

private:
    static constexpr size_t kMaxPoints = 300;

    std::unordered_map<int64_t, std::deque<QPointF>> history_;
    QChart* chart_;
    QLineSeries* price_;
    QLineSeries* threshold_;
    QDateTimeAxis* axisX_;
    QValueAxis* axisY_;
};
