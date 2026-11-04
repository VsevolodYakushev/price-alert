#include "ui/PriceChart.h"

#include <QChart>
#include <QDateTime>
#include <QDateTimeAxis>
#include <QLegend>
#include <QLineSeries>
#include <QValueAxis>

#include <algorithm>
#include <unordered_set>

#include "ui/Format.h"

namespace {
const QColor kPanel("#171A21");
const QColor kBackground("#0F1115");
const QColor kText("#E6E8EB");
const QColor kMutedText("#8B93A1");
const QColor kGrid("#262B36");
const QColor kAccent("#3B82F6");
const QColor kDown("#EA3943");
}  // namespace

PriceChart::PriceChart(QWidget* parent)
    : QChartView(parent),
      chart_(new QChart),
      price_(new QLineSeries),
      threshold_(new QLineSeries),
      axisX_(new QDateTimeAxis),
      axisY_(new QValueAxis) {
    price_->setName("Цена");
    QPen pricePen(kAccent);
    pricePen.setWidthF(2.0);
    price_->setPen(pricePen);

    threshold_->setName("Цель");
    QPen thresholdPen(kDown);
    thresholdPen.setWidthF(1.5);
    thresholdPen.setStyle(Qt::DashLine);
    threshold_->setPen(thresholdPen);

    chart_->addSeries(price_);
    chart_->addSeries(threshold_);

    axisX_->setFormat("hh:mm:ss");
    axisX_->setTickCount(6);
    for (QAbstractAxis* axis : {static_cast<QAbstractAxis*>(axisX_), static_cast<QAbstractAxis*>(axisY_)}) {
        axis->setLabelsBrush(kMutedText);
        axis->setGridLineColor(kGrid);
        axis->setLinePenColor(kGrid);
    }
    chart_->addAxis(axisX_, Qt::AlignBottom);
    chart_->addAxis(axisY_, Qt::AlignRight);
    for (QLineSeries* series : {price_, threshold_}) {
        series->attachAxis(axisX_);
        series->attachAxis(axisY_);
    }

    chart_->setBackgroundBrush(kPanel);
    chart_->setBackgroundRoundness(8);
    chart_->setMargins(QMargins(8, 8, 8, 8));
    chart_->setTitleBrush(kText);
    QFont titleFont = chart_->titleFont();
    titleFont.setPointSize(11);
    titleFont.setBold(true);
    chart_->setTitleFont(titleFont);
    chart_->legend()->setAlignment(Qt::AlignTop);
    chart_->legend()->setLabelColor(kMutedText);

    setChart(chart_);
    setRenderHint(QPainter::Antialiasing);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(kBackground);
    setMinimumHeight(220);

    display(nullptr);
}

void PriceChart::appendHistory(const std::vector<AssetSnapshot>& rows) {
    std::unordered_set<int64_t> alive;
    for (const auto& s : rows) {
        alive.insert(s.asset.id);
        if (!s.last_price)
            continue;
        const qreal ms = static_cast<qreal>(Format::toMs(s.ts));
        auto& points = history_[s.asset.id];
        if (!points.empty() && points.back().x() >= ms)
            continue;
        points.emplace_back(ms, *s.last_price);
        if (points.size() > kMaxPoints)
            points.pop_front();
    }
    for (auto it = history_.begin(); it != history_.end();)
        it = alive.count(it->first) ? std::next(it) : history_.erase(it);
}

void PriceChart::display(const AssetSnapshot* selected) {
    const auto it = selected ? history_.find(selected->asset.id) : history_.end();
    if (!selected || it == history_.end() || it->second.empty()) {
        price_->clear();
        threshold_->clear();
        chart_->legend()->hide();
        axisX_->hide();
        axisY_->hide();
        chart_->setTitle(selected ? Format::qs(selected->asset.symbol) + " — ждём первые котировки…"
                                  : "Выберите актив в таблице");
        return;
    }

    const auto& points = it->second;
    const double target = selected->asset.target_price;
    price_->replace(QList<QPointF>(points.begin(), points.end()));

    qreal minX = points.front().x();
    qreal maxX = points.back().x();
    if (maxX - minX < 1000)
        maxX = minX + 1000;
    threshold_->replace(QList<QPointF>{{minX, target}, {maxX, target}});

    const auto [lo, hi] = std::minmax_element(points.begin(), points.end(),
                                              [](const QPointF& a, const QPointF& b) { return a.y() < b.y(); });
    double minY = std::min(lo->y(), target);
    double maxY = std::max(hi->y(), target);
    const double pad = maxY > minY ? (maxY - minY) * 0.1 : maxY * 0.001;
    minY -= pad;
    maxY += pad;

    axisX_->setRange(QDateTime::fromMSecsSinceEpoch(qint64(minX)), QDateTime::fromMSecsSinceEpoch(qint64(maxX)));
    axisY_->setRange(minY, maxY);
    axisY_->setLabelFormat(maxY >= 1.0 ? "%.2f" : "%.6f");
    axisX_->show();
    axisY_->show();
    chart_->legend()->show();
    chart_->setTitle(Format::qs(selected->asset.symbol));
}

void PriceChart::clear() {
    history_.clear();
    display(nullptr);
}
