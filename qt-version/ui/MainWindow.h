#pragma once

#include <QMainWindow>

#include <optional>

#include "core_api/AuthService.h"
#include "core_api/PortfolioService.h"

class AssetTableModel;
class PriceChart;
class QLabel;
class QLineEdit;
class QListWidget;
class QTableView;
class QTimer;

// Дашборд: портфель, график выбранного актива, журнал алертов.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(AuthService& auth, PortfolioService& portfolio, QWidget* parent = nullptr);

    void startSession();  // после входа
    void stopSession();   // после выхода
    void addAlertToLog(const Alert& alert);
    // Если трей есть, крестик сворачивает окно туда, а не закрывает приложение.
    void setCloseToTray(bool enabled);

signals:
    void logoutRequested();
    void minimizedToTray();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void refresh();
    void addAsset();
    void removeSelected();
    void onThresholdEdited(int64_t id, double price);
    void onSelectionChanged();
    void restoreSelection(std::optional<int64_t> id);
    const AssetSnapshot* selectedSnapshot() const;

    AuthService& auth_;
    PortfolioService& portfolio_;

    AssetTableModel* model_;
    QTimer* timer_;
    QLabel* userLabel_ = nullptr;
    QLineEdit* symbolEdit_ = nullptr;
    QLineEdit* targetEdit_ = nullptr;
    QLabel* formMessage_ = nullptr;
    QTableView* table_ = nullptr;
    PriceChart* chart_ = nullptr;
    QListWidget* alertLog_ = nullptr;
    QLabel* statusLabel_ = nullptr;

    std::optional<int64_t> selectedId_;
    bool closeToTray_ = false;
};
