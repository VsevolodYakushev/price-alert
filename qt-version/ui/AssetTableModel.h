#pragma once

#include <QAbstractTableModel>

#include <vector>

#include "core_api/Types.h"

// Таблица портфеля поверх копии snapshot(). Порог редактируется прямо в ячейке.
class AssetTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    enum Column { Symbol, Target, Price, Delta, Enabled, ColumnCount };

    using QAbstractTableModel::QAbstractTableModel;

    // Возвращает true, если поменялся состав строк (модель сброшена, выделение потеряно).
    bool setSnapshot(std::vector<AssetSnapshot> rows);
    const AssetSnapshot* at(int row) const;
    int rowForId(int64_t id) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

    // Цена опустилась до цели.
    static bool isReached(const AssetSnapshot& s);

signals:
    void thresholdEdited(int64_t id, double price);

private:
    std::vector<AssetSnapshot> rows_;
};
