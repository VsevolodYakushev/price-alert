#include "ui/MainWindow.h"

#include <QCloseEvent>
#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTime>
#include <QTimer>
#include <QVBoxLayout>

#include "ui/AssetTableModel.h"
#include "ui/Format.h"
#include "ui/PriceChart.h"

namespace {

const QRegularExpression kPriceInput(R"(\d{1,10}([.,]\d{0,8})?)");

// Редактор цели прямо в ячейке: принимает и точку, и запятую.
class PriceDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&, const QModelIndex&) const override {
        auto* edit = new QLineEdit(parent);
        edit->setValidator(new QRegularExpressionValidator(kPriceInput, edit));
        return edit;
    }
    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        static_cast<QLineEdit*>(editor)->setText(QString::number(index.data(Qt::EditRole).toDouble(), 'g', 12));
    }
    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override {
        model->setData(index, static_cast<QLineEdit*>(editor)->text(), Qt::EditRole);
    }
};

}  // namespace

MainWindow::MainWindow(AuthService& auth, PortfolioService& portfolio, QWidget* parent)
    : QMainWindow(parent),
      auth_(auth),
      portfolio_(portfolio),
      model_(new AssetTableModel(this)),
      timer_(new QTimer(this)) {
    setWindowTitle("Price Alert");
    resize(1120, 740);

    auto* central = new QWidget;
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(16, 12, 16, 8);
    root->setSpacing(12);

    // Шапка: название, пользователь, выход.
    auto* title = new QLabel("Price Alert");
    title->setObjectName("appTitle");
    userLabel_ = new QLabel;
    userLabel_->setObjectName("muted");
    auto* logoutButton = new QPushButton("Выйти");
    logoutButton->setCursor(Qt::PointingHandCursor);
    auto* header = new QHBoxLayout;
    header->addWidget(title);
    header->addStretch();
    header->addWidget(userLabel_);
    header->addWidget(logoutButton);
    root->addLayout(header);

    // Панель добавления актива.
    symbolEdit_ = new QLineEdit;
    symbolEdit_->setPlaceholderText("Тикер, например BTCUSDT");
    symbolEdit_->setValidator(new QRegularExpressionValidator(QRegularExpression("[A-Za-z0-9]{0,20}"), symbolEdit_));
    symbolEdit_->setFixedWidth(220);
    targetEdit_ = new QLineEdit;
    targetEdit_->setPlaceholderText("Целевая цена");
    targetEdit_->setValidator(new QRegularExpressionValidator(kPriceInput, targetEdit_));
    targetEdit_->setFixedWidth(160);
    auto* addButton = new QPushButton("Добавить");
    addButton->setObjectName("primary");
    addButton->setCursor(Qt::PointingHandCursor);
    auto* removeButton = new QPushButton("Удалить выбранный");
    removeButton->setCursor(Qt::PointingHandCursor);
    formMessage_ = new QLabel;
    formMessage_->setObjectName("formMessage");

    auto* panel = new QFrame;
    panel->setObjectName("panel");
    auto* panelLayout = new QHBoxLayout(panel);
    panelLayout->setContentsMargins(12, 10, 12, 10);
    panelLayout->setSpacing(8);
    panelLayout->addWidget(symbolEdit_);
    panelLayout->addWidget(targetEdit_);
    panelLayout->addWidget(addButton);
    panelLayout->addWidget(formMessage_, 1);
    panelLayout->addWidget(removeButton);
    root->addWidget(panel);

    // Таблица портфеля.
    table_ = new QTableView;
    table_->setModel(model_);
    table_->setItemDelegateForColumn(AssetTableModel::Target, new PriceDelegate(table_));
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table_->setAlternatingRowColors(true);
    table_->setShowGrid(false);
    table_->verticalHeader()->hide();
    table_->verticalHeader()->setDefaultSectionSize(34);
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->horizontalHeader()->setHighlightSections(false);

    // График и журнал алертов.
    chart_ = new PriceChart;
    alertLog_ = new QListWidget;
    auto* logTitle = new QLabel("Журнал алертов");
    logTitle->setObjectName("sectionTitle");
    auto* logBox = new QWidget;
    auto* logLayout = new QVBoxLayout(logBox);
    logLayout->setContentsMargins(0, 0, 0, 0);
    logLayout->addWidget(logTitle);
    logLayout->addWidget(alertLog_);

    auto* bottom = new QSplitter(Qt::Horizontal);
    bottom->addWidget(chart_);
    bottom->addWidget(logBox);
    bottom->setStretchFactor(0, 3);
    bottom->setStretchFactor(1, 1);

    auto* split = new QSplitter(Qt::Vertical);
    split->addWidget(table_);
    split->addWidget(bottom);
    split->setStretchFactor(0, 2);
    split->setStretchFactor(1, 3);
    root->addWidget(split, 1);

    setCentralWidget(central);
    statusLabel_ = new QLabel;
    statusBar()->addWidget(statusLabel_);

    connect(logoutButton, &QPushButton::clicked, this, &MainWindow::logoutRequested);
    connect(addButton, &QPushButton::clicked, this, &MainWindow::addAsset);
    connect(symbolEdit_, &QLineEdit::returnPressed, this, &MainWindow::addAsset);
    connect(targetEdit_, &QLineEdit::returnPressed, this, &MainWindow::addAsset);
    connect(symbolEdit_, &QLineEdit::textEdited, this, [this](const QString& text) {
        const int cursor = symbolEdit_->cursorPosition();
        symbolEdit_->setText(text.toUpper());
        symbolEdit_->setCursorPosition(cursor);
    });
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::removeSelected);
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, table_);
    connect(deleteShortcut, &QShortcut::activated, this, &MainWindow::removeSelected);
    connect(model_, &AssetTableModel::thresholdEdited, this, &MainWindow::onThresholdEdited);
    connect(table_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &MainWindow::onSelectionChanged);
    connect(timer_, &QTimer::timeout, this, &MainWindow::refresh);
}

void MainWindow::startSession() {
    const auto user = auth_.currentUser();
    userLabel_->setText(user ? Format::qs(user->username) : QString());
    selectedId_.reset();
    chart_->clear();
    alertLog_->clear();
    formMessage_->clear();
    refresh();
    timer_->start(1000);
}

void MainWindow::stopSession() {
    timer_->stop();
    selectedId_.reset();
    model_->setSnapshot({});
    chart_->clear();
    alertLog_->clear();
    formMessage_->clear();
    statusLabel_->clear();
}

void MainWindow::addAlertToLog(const Alert& alert) {
    auto* item = new QListWidgetItem(QString("%1   %2   %3 ≤ %4")
                                         .arg(Format::time(alert.ts), Format::qs(alert.symbol),
                                              Format::price(alert.price), Format::price(alert.threshold)));
    item->setForeground(QColor("#EA3943"));
    item->setFont(Format::monoFont());
    alertLog_->insertItem(0, item);
    while (alertLog_->count() > 200)
        delete alertLog_->takeItem(alertLog_->count() - 1);
}

void MainWindow::setCloseToTray(bool enabled) {
    closeToTray_ = enabled;
}

void MainWindow::closeEvent(QCloseEvent* event) {
    if (closeToTray_) {
        hide();
        event->ignore();
        emit minimizedToTray();
        return;
    }
    event->accept();
    QCoreApplication::quit();
}

void MainWindow::refresh() {
    const auto keep = selectedId_;
    const auto rows = portfolio_.snapshot();
    if (model_->setSnapshot(rows))
        restoreSelection(keep);
    chart_->appendHistory(rows);
    chart_->display(selectedSnapshot());
    statusLabel_->setText(QString("Активов: %1 · обновлено %2")
                              .arg(rows.size())
                              .arg(QTime::currentTime().toString("hh:mm:ss")));
}

void MainWindow::addAsset() {
    static const QRegularExpression symbolRe("^[A-Z0-9]{5,20}$");
    const QString symbol = symbolEdit_->text().trimmed().toUpper();
    if (!symbolRe.match(symbol).hasMatch()) {
        Format::setFormMessage(formMessage_, "Тикер как на Binance: 5–20 латинских букв и цифр, например BTCUSDT", true);
        symbolEdit_->setFocus();
        return;
    }
    bool ok = false;
    const double target = targetEdit_->text().replace(',', '.').toDouble(&ok);
    if (!ok || target <= 0.0) {
        Format::setFormMessage(formMessage_, "Укажите целевую цену больше нуля", true);
        targetEdit_->setFocus();
        return;
    }

    const auto id = portfolio_.addAsset(symbol.toStdString(), target);
    if (!id) {
        Format::setFormMessage(formMessage_, QString("Не удалось добавить %1: тикер уже в портфеле или недоступен").arg(symbol), true);
        return;
    }
    Format::setFormMessage(formMessage_, QString("%1 добавлен").arg(symbol), false);
    symbolEdit_->clear();
    targetEdit_->clear();
    symbolEdit_->setFocus();
    selectedId_ = *id;  // refresh() выделит новую строку
    refresh();
}

void MainWindow::removeSelected() {
    const AssetSnapshot* selected = selectedSnapshot();
    if (!selected) {
        Format::setFormMessage(formMessage_, "Выберите актив в таблице", true);
        return;
    }
    const QString symbol = Format::qs(selected->asset.symbol);
    portfolio_.removeAsset(selected->asset.id);
    selectedId_.reset();
    Format::setFormMessage(formMessage_, QString("%1 удалён").arg(symbol), false);
    refresh();
}

void MainWindow::onThresholdEdited(int64_t id, double price) {
    portfolio_.setThreshold(id, price);
    refresh();
}

void MainWindow::onSelectionChanged() {
    const auto rows = table_->selectionModel()->selectedRows();
    const AssetSnapshot* s = rows.isEmpty() ? nullptr : model_->at(rows.first().row());
    selectedId_ = s ? std::optional<int64_t>(s->asset.id) : std::nullopt;
    chart_->display(s);
}

void MainWindow::restoreSelection(std::optional<int64_t> id) {
    selectedId_.reset();
    const int row = id ? model_->rowForId(*id) : -1;
    if (row >= 0)
        table_->selectRow(row);  // onSelectionChanged обновит selectedId_
}

const AssetSnapshot* MainWindow::selectedSnapshot() const {
    return selectedId_ ? model_->at(model_->rowForId(*selectedId_)) : nullptr;
}
