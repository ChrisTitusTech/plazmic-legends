#include "ui/main_window.h"

#include "ui/map_canvas.h"
#include "ui/spawn_table_model.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

#include <QAbstractItemView>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QScreen>
#include <QStatusBar>
#include <QTableView>
#include <QTimer>
#include <QVBoxLayout>

namespace plazmic {
namespace {

QDockWidget* create_dock(const QString& title,
                         const QString& object_name,
                         QWidget* content,
                         QWidget* parent) {
    auto* dock = new QDockWidget(title, parent);
    dock->setObjectName(object_name);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    dock->setFeatures(QDockWidget::DockWidgetClosable |
                      QDockWidget::DockWidgetMovable |
                      QDockWidget::DockWidgetFloatable);
    dock->setWidget(content);
    return dock;
}

QString pid_text(const StatusSnapshot& snapshot) {
    const std::string_view label = process_label(snapshot.process);
    const QString process =
        QString::fromUtf8(label.data(), static_cast<qsizetype>(label.size()));
    if (!snapshot.pid) {
        return process;
    }
    return QString("%1 (PID %2)")
        .arg(process)
        .arg(*snapshot.pid);
}

}  // namespace

MainWindow::MainWindow(StatusSnapshot snapshot,
                       QString settings_path,
                       bool reset_layout,
                       QWidget* parent)
    : QMainWindow(parent),
      snapshot_(std::move(snapshot)),
      settings_(std::move(settings_path)),
      reset_layout_(reset_layout) {
    build_ui();
    update_snapshot(snapshot_);
    restore_ui_state();
}

void MainWindow::build_ui() {
    setObjectName("plazmic-main-window");
    setWindowTitle("Plazmic Legends");
    setDockOptions(QMainWindow::AnimatedDocks |
                   QMainWindow::AllowNestedDocks |
                   QMainWindow::AllowTabbedDocks);
    resize(1200, 780);

    map_canvas_ = new MapCanvas;
    map_canvas_->setObjectName("map-view");
    setCentralWidget(map_canvas_);

    auto* spawn_container = new QWidget;
    auto* spawn_layout = new QVBoxLayout(spawn_container);
    spawn_state_ = new QLabel("Waiting for live spawn data");
    spawn_state_->setObjectName("spawn-state");
    spawn_state_->setWordWrap(true);
    spawn_layout->addWidget(spawn_state_);

    auto* filter_row = new QHBoxLayout;
    spawn_filter_ = new QLineEdit;
    spawn_filter_->setObjectName("spawn-filter");
    spawn_filter_->setPlaceholderText("Filter names");
    spawn_filter_->setMaxLength(256);
    filter_row->addWidget(spawn_filter_, 1);
    spawn_type_filter_ = new QComboBox;
    spawn_type_filter_->setObjectName("spawn-type-filter");
    spawn_type_filter_->addItem("All types", -1);
    spawn_type_filter_->addItem("Players", 0);
    spawn_type_filter_->addItem("NPCs", 1);
    spawn_type_filter_->addItem("Corpses", 2);
    filter_row->addWidget(spawn_type_filter_);
    spawn_layout->addLayout(filter_row);

    spawn_model_ = new SpawnTableModel(this);
    spawn_proxy_ = new SpawnFilterProxyModel(this);
    spawn_proxy_->setSourceModel(spawn_model_);
    spawn_table_ = new QTableView;
    spawn_table_->setObjectName("spawn-table");
    spawn_table_->setModel(spawn_proxy_);
    spawn_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    spawn_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    spawn_table_->setSortingEnabled(true);
    spawn_table_->setAlternatingRowColors(true);
    spawn_table_->verticalHeader()->setVisible(false);
    spawn_table_->horizontalHeader()->setStretchLastSection(true);
    spawn_table_->sortByColumn(
        SpawnTableModel::distance_column, Qt::AscendingOrder);
    spawn_layout->addWidget(spawn_table_);
    auto* spawn_dock = create_dock(
        "Spawns", "spawn-dock", spawn_container, this);
    addDockWidget(Qt::RightDockWidgetArea, spawn_dock);

    selection_detail_ = new QLabel("No spawn selected.");
    selection_detail_->setObjectName("selection-detail");
    selection_detail_->setAlignment(Qt::AlignCenter);
    selection_detail_->setWordWrap(true);
    auto* detail_dock = create_dock(
        "Details", "detail-dock", selection_detail_, this);
    addDockWidget(Qt::BottomDockWidgetArea, detail_dock);

    connect(
        spawn_filter_, &QLineEdit::textChanged, spawn_proxy_,
        &SpawnFilterProxyModel::set_name_filter);
    connect(
        spawn_type_filter_, &QComboBox::currentIndexChanged, this,
        [this](int index) {
            const int value =
                spawn_type_filter_->itemData(index).toInt();
            std::optional<SpawnType> type;
            if (value >= 0 && value <= 2) {
                type = static_cast<SpawnType>(value);
            }
            spawn_proxy_->set_type_filter(type);
        });
    connect(
        spawn_table_->selectionModel(),
        &QItemSelectionModel::currentRowChanged, this,
        [this](const QModelIndex& current) {
            if (!current.isValid()) {
                clear_spawn_selection();
                return;
            }
            const std::uint32_t id =
                current.data(kSpawnIdRole).toUInt();
            const int source_row =
                spawn_proxy_->mapToSource(current).row();
            const SpawnSnapshot* spawn =
                spawn_model_->spawn_at(source_row);
            selected_spawn_ = id;
            map_canvas_->set_selected_spawn(id);
            update_spawn_detail(spawn);
        });
    map_canvas_->set_spawn_selected_callback(
        [this](std::uint32_t id) { select_spawn(id); });

    compatibility_value_ = new QLabel;
    compatibility_value_->setObjectName("compatibility-status");
    process_value_ = new QLabel;
    process_value_->setObjectName("process-status");
    profile_value_ = new QLabel;
    profile_value_->setObjectName("profile-status");
    detail_value_ = new QLabel;
    detail_value_->setObjectName("detail-status");

    statusBar()->addWidget(new QLabel("Client:"));
    statusBar()->addWidget(compatibility_value_);
    statusBar()->addWidget(new QLabel("Process:"));
    statusBar()->addWidget(process_value_);
    statusBar()->addWidget(new QLabel("Profile:"));
    statusBar()->addWidget(profile_value_);
    statusBar()->addPermanentWidget(detail_value_, 1);
}

void MainWindow::update_snapshot(const StatusSnapshot& snapshot) {
    snapshot_ = snapshot;
    const std::string_view label =
        compatibility_label(snapshot.compatibility);
    compatibility_value_->setText(QString::fromUtf8(
        label.data(), static_cast<qsizetype>(label.size())));
    process_value_->setText(pid_text(snapshot));
    profile_value_->setText(QString::fromStdString(snapshot.profile));
    detail_value_->setText(QString::fromStdString(snapshot.detail));
}

void MainWindow::update_player_snapshot(const PlayerSnapshot& snapshot) {
    map_canvas_->set_player_snapshot(snapshot);
    if (!snapshot.detail.empty()) {
        detail_value_->setText(QString::fromStdString(snapshot.detail));
    }
}

void MainWindow::update_spawn_snapshot(
    SpawnCollectionSnapshot snapshot) {
    const std::optional<std::uint32_t> retained = selected_spawn_;
    map_canvas_->set_spawn_snapshot(snapshot);
    spawn_model_->set_snapshot(std::move(snapshot));
    if (spawn_model_->snapshot().available()) {
        spawn_state_->setText(
            QString("%1 live spawns")
                .arg(spawn_model_->snapshot().spawns.size()));
    } else {
        spawn_state_->setText(
            QString::fromStdString(spawn_model_->snapshot().detail));
    }
    if (retained && spawn_model_->row_for_id(*retained) >= 0) {
        select_spawn(*retained);
    } else {
        clear_spawn_selection();
    }
}

void MainWindow::select_spawn(std::uint32_t id) {
    const int source_row = spawn_model_->row_for_id(id);
    if (source_row < 0) {
        clear_spawn_selection();
        return;
    }
    const QModelIndex source =
        spawn_model_->index(source_row, 0);
    QModelIndex proxy = spawn_proxy_->mapFromSource(source);
    if (!proxy.isValid()) {
        spawn_filter_->clear();
        spawn_type_filter_->setCurrentIndex(0);
        proxy = spawn_proxy_->mapFromSource(source);
    }
    if (!proxy.isValid()) {
        clear_spawn_selection();
        return;
    }
    selected_spawn_ = id;
    spawn_table_->setCurrentIndex(proxy);
    spawn_table_->selectRow(proxy.row());
    spawn_table_->scrollTo(proxy);
    map_canvas_->set_selected_spawn(id);
    update_spawn_detail(spawn_model_->spawn_at(source_row));
}

void MainWindow::clear_spawn_selection() {
    selected_spawn_.reset();
    if (spawn_table_->selectionModel() != nullptr) {
        spawn_table_->selectionModel()->clearSelection();
    }
    map_canvas_->set_selected_spawn(std::nullopt);
    update_spawn_detail(nullptr);
}

void MainWindow::update_spawn_detail(const SpawnSnapshot* spawn) {
    if (spawn == nullptr) {
        selection_detail_->setText("No spawn selected.");
        return;
    }
    const QString name =
        QString::fromStdString(spawn->name).toHtmlEscaped();
    selection_detail_->setText(
        QString("<h3>%1</h3>"
                "<p>Level %2 %3 - ID %4</p>"
                "<p>X %5 - Y %6 - Z %7 - Distance %8</p>")
            .arg(name)
            .arg(spawn->level)
            .arg(QString::fromLatin1(spawn_type_label(spawn->type)))
            .arg(spawn->id)
            .arg(spawn->x, 0, 'f', 1)
            .arg(spawn->y, 0, 'f', 1)
            .arg(spawn->z, 0, 'f', 1)
            .arg(spawn->distance, 0, 'f', 1));
}

void MainWindow::set_zone_map(ZoneMap map) {
    map_canvas_->set_zone_map(std::move(map));
}

void MainWindow::clear_zone_map(const QString& detail) {
    map_canvas_->clear_zone_map(detail);
}

void MainWindow::restore_ui_state() {
    if (!reset_layout_) {
        if (const auto state = settings_.load()) {
            const bool layout_restored = restoreState(state->layout);
            const bool geometry_restored = restoreGeometry(state->geometry);
            map_canvas_->set_height_filter_range(
                state->height_filter_below,
                state->height_filter_above);
            map_canvas_->set_height_filter_enabled(
                state->height_filter_enabled);
            map_canvas_->set_player_follow_enabled(
                state->player_follow_enabled);
            spawn_filter_->setText(state->spawn_filter);
            const int type_index =
                spawn_type_filter_->findData(state->spawn_type_filter);
            spawn_type_filter_->setCurrentIndex(
                type_index >= 0 ? type_index : 0);
            for (int column = 0;
                 column < SpawnTableModel::column_count; ++column) {
                spawn_table_->setColumnWidth(
                    column,
                    state->spawn_column_widths[
                        static_cast<std::size_t>(column)]);
            }
            spawn_table_->sortByColumn(
                state->spawn_sort_column,
                state->spawn_sort_descending
                    ? Qt::DescendingOrder
                    : Qt::AscendingOrder);
            if (!geometry_restored || !layout_restored) {
                resize(1200, 780);
            }
        }
    }
    QTimer::singleShot(0, this, [this]() { ensure_on_screen(); });
}

void MainWindow::ensure_on_screen() {
    const QRect frame = frameGeometry();
    const bool visible = std::ranges::any_of(
        QGuiApplication::screens(), [&frame](const QScreen* screen) {
            const QRect overlap = frame.intersected(screen->availableGeometry());
            return overlap.width() >= 100 && overlap.height() >= 100;
        });
    if (visible) {
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (screen == nullptr) {
        return;
    }
    resize(1200, 780);
    const QRect available = screen->availableGeometry();
    move(available.center() - rect().center());
}

void MainWindow::save_ui_state() {
    std::array<int, 4> column_widths{};
    for (int column = 0;
         column < SpawnTableModel::column_count; ++column) {
        column_widths[static_cast<std::size_t>(column)] =
            spawn_table_->columnWidth(column);
    }
    const int type_filter =
        spawn_type_filter_
            ->itemData(spawn_type_filter_->currentIndex())
            .toInt();
    const QHeaderView* header = spawn_table_->horizontalHeader();
    const UiState state{
        .geometry = saveGeometry(),
        .layout = saveState(),
        .height_filter_enabled =
            map_canvas_->height_filter_enabled(),
        .height_filter_below =
            map_canvas_->height_filter_below(),
        .height_filter_above =
            map_canvas_->height_filter_above(),
        .player_follow_enabled =
            map_canvas_->player_follow_enabled(),
        .spawn_filter = spawn_filter_->text(),
        .spawn_type_filter = type_filter,
        .spawn_sort_column = header->sortIndicatorSection(),
        .spawn_sort_descending =
            header->sortIndicatorOrder() == Qt::DescendingOrder,
        .spawn_column_widths = column_widths,
    };
    (void)settings_.save(state);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    save_ui_state();
    QMainWindow::closeEvent(event);
    QCoreApplication::quit();
}

}  // namespace plazmic
