#include "ui/AssetTableModel.h"

#include <QColor>
#include <QFont>

#include <algorithm>

#include "ui/Format.h"

namespace {
const QColor kUp("#16C784");
const QColor kDown("#EA3943");
const QColor kMuted("#5B6270");
const QColor kReachedRow(234, 57, 67, 45);
}  // namespace

bool AssetTableModel::isReached(const AssetSnapshot& s) {
    return s.last_price && *s.last_price <= s.asset.target_price;
}

bool AssetTableModel::setSnapshot(std::vector<AssetSnapshot> rows) {
    const bool sameRows = rows.size() == rows_.size() &&
                          std::equal(rows.begin(), rows.end(), rows_.begin(),
                                     [](const AssetSnapshot& a, const AssetSnapshot& b) {
                                         return a.asset.id == b.asset.id;
                                     });
    if (!sameRows) {
        beginResetModel();
        rows_ = std::move(rows);
        endResetModel();
        return true;
    }

    for (size_t i = 0; i < rows.size(); ++i) {
        const bool targetChanged = rows[i].asset.target_price != rows_[i].asset.target_price ||
                                   isReached(rows[i]) != isReached(rows_[i]);
        rows_[i] = std::move(rows[i]);
        const int r = static_cast<int>(i);
        // Колонку «Цель» обновляем только при изменении, иначе раз в секунду
        // сбрасывался бы открытый редактор порога.
        emit dataChanged(index(r, Symbol), index(r, Symbol));
        emit dataChanged(index(r, Price), index(r, Enabled));
        if (targetChanged)
            emit dataChanged(index(r, Target), index(r, Target));
    }
    return false;
}

const AssetSnapshot* AssetTableModel::at(int row) const {
    if (row < 0 || row >= static_cast<int>(rows_.size()))
        return nullptr;
    return &rows_[row];
}

int AssetTableModel::rowForId(int64_t id) const {
    for (size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].asset.id == id)
            return static_cast<int>(i);
    return -1;
}

int AssetTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(rows_.size());
}

int AssetTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant AssetTableModel::data(const QModelIndex& index, int role) const {
    const AssetSnapshot* s = index.isValid() ? at(index.row()) : nullptr;
    if (!s)
        return {};
    const int column = index.column();
    const bool numeric = column == Target || column == Price || column == Delta;

    switch (role) {
    case Qt::DisplayRole:
        switch (column) {
        case Symbol:
            return Format::qs(s->asset.symbol);
        case Target:
            return Format::price(s->asset.target_price);
        case Price:
            return s->last_price ? Format::price(*s->last_price) : QString("—");
        case Delta:
            if (!s->last_price)
                return QString("—");
            return Format::percent((*s->last_price - s->asset.target_price) / s->asset.target_price * 100.0);
        case Enabled:
            return s->asset.enabled ? QString("да") : QString("нет");
        }
        break;
    case Qt::EditRole:
        if (column == Target)
            return s->asset.target_price;
        break;
    case Qt::TextAlignmentRole:
        if (numeric)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        if (column == Enabled)
            return int(Qt::AlignCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    case Qt::FontRole:
        if (numeric)
            return Format::monoFont();
        if (column == Symbol) {
            QFont font;
            font.setBold(true);
            return font;
        }
        break;
    case Qt::ForegroundRole:
        if (!s->asset.enabled)
            return kMuted;
        if (column == Delta && s->last_price)
            return isReached(*s) ? kDown : kUp;
        break;
    case Qt::BackgroundRole:
        if (isReached(*s))
            return kReachedRow;
        break;
    case Qt::ToolTipRole:
        if (column == Target)
            return QString("Двойной щелчок — изменить цель");
        break;
    }
    return {};
}

QVariant AssetTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal)
        return {};
    if (role == Qt::TextAlignmentRole)
        return int((section == Target || section == Price || section == Delta ? Qt::AlignRight : Qt::AlignLeft) |
                   Qt::AlignVCenter);
    if (role != Qt::DisplayRole)
        return {};
    switch (section) {
    case Symbol:
        return QString("Тикер");
    case Target:
        return QString("Цель");
    case Price:
        return QString("Цена");
    case Delta:
        return QString("Δ до цели");
    case Enabled:
        return QString("Активен");
    }
    return {};
}

Qt::ItemFlags AssetTableModel::flags(const QModelIndex& index) const {
    auto f = QAbstractTableModel::flags(index);
    if (index.isValid() && index.column() == Target)
        f |= Qt::ItemIsEditable;
    return f;
}

bool AssetTableModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (role != Qt::EditRole || index.column() != Target || !at(index.row()))
        return false;
    bool ok = false;
    const double price = value.toString().replace(',', '.').toDouble(&ok);
    if (!ok || price <= 0.0)
        return false;

    auto& row = rows_[index.row()];
    row.asset.target_price = price;
    emit dataChanged(this->index(index.row(), 0), this->index(index.row(), ColumnCount - 1));
    emit thresholdEdited(row.asset.id, price);
    return true;
}
