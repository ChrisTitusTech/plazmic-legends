#include "ui/main_window.h"

#include "ui/map_canvas.h"
#include "ui/spawn_presentation.h"
#include "ui/spawn_table_model.h"

#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

#include <QAbstractItemView>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCoreApplication>
#include <QDockWidget>
#include <QEvent>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QProgressBar>
#include <QScreen>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QTimer>
#include <QToolButton>
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

void update_vital_bar(QProgressBar* bar,
                      const QString& label,
                      const VitalSnapshot& vital) {
    if (!vital.maximum) {
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFormat(
            QString("%1 %2").arg(label).arg(vital.current));
        return;
    }
    if (*vital.maximum == 0) {
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setFormat(QString("%1 0 / 0").arg(label));
        return;
    }
    const std::int64_t maximum = *vital.maximum;
    const double ratio =
        static_cast<double>(vital.current) /
        static_cast<double>(maximum) * 100.0;
    const int percentage = static_cast<int>(std::clamp(ratio, 0.0, 100.0));
    bar->setRange(0, 100);
    bar->setValue(percentage);
    bar->setFormat(QString("%1 %2 / %3 (%4%)")
                       .arg(label)
                       .arg(vital.current)
                       .arg(maximum)
                       .arg(percentage));
}

void set_vital_unavailable(QProgressBar* bar, const QString& label) {
    bar->setRange(0, 100);
    bar->setValue(0);
    bar->setFormat(QString("%1 unavailable").arg(label));
}

}  // namespace

MainWindow::MainWindow(StatusSnapshot snapshot,
                       QString settings_path,
                       bool reset_layout,
                       QString client_directory,
                       QWidget* parent)
    : QMainWindow(parent),
      snapshot_(std::move(snapshot)),
      settings_(std::move(settings_path)),
      reset_layout_(reset_layout),
      client_directory_(std::move(client_directory)) {
    build_ui();
    update_snapshot(snapshot_);
    restore_ui_state();
}

void MainWindow::build_ui() {
    setObjectName("plazmic-main-window");
    const QString version = QCoreApplication::applicationVersion();
    setWindowTitle(
        version.isEmpty()
            ? QString("Plazmic Legends")
            : QString("Plazmic Legends %1").arg(version));
    setDockOptions(QMainWindow::AnimatedDocks |
                   QMainWindow::AllowNestedDocks |
                   QMainWindow::AllowTabbedDocks);
    resize(1200, 780);

    map_canvas_ = new MapCanvas;
    map_canvas_->setObjectName("map-view");
    setCentralWidget(map_canvas_);

    auto* character_container = new QWidget;
    auto* character_layout = new QVBoxLayout(character_container);
    character_layout->setContentsMargins(6, 6, 6, 6);
    character_name_ = new QLabel("Character unavailable");
    character_name_->setObjectName("character-name");
    QFont character_font = character_name_->font();
    character_font.setBold(true);
    character_name_->setFont(character_font);
    character_layout->addWidget(character_name_);
    health_bar_ = new QProgressBar;
    health_bar_->setObjectName("character-health");
    health_bar_->setTextVisible(true);
    character_layout->addWidget(health_bar_);
    mana_bar_ = new QProgressBar;
    mana_bar_->setObjectName("character-mana");
    mana_bar_->setTextVisible(true);
    character_layout->addWidget(mana_bar_);
    current_dps_ = new QLabel("Current DPS: 0.0");
    current_dps_->setObjectName("character-dps");
    character_layout->addWidget(current_dps_);
    equipment_table_ = new QTableWidget;
    equipment_table_->setObjectName("equipment-table");
    equipment_table_->setColumnCount(2);
    equipment_table_->setHorizontalHeaderLabels({"Slot", "Item"});
    equipment_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    equipment_table_->setSelectionMode(QAbstractItemView::NoSelection);
    equipment_table_->setAlternatingRowColors(true);
    equipment_table_->verticalHeader()->setVisible(false);
    equipment_table_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    equipment_table_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    character_layout->addWidget(equipment_table_, 1);
    auto* character_dock = create_dock(
        "Character", "character-dock", character_container, this);
    character_dock->setMinimumWidth(300);
    addDockWidget(Qt::LeftDockWidgetArea, character_dock);

    auto* parse_container = new QWidget;
    auto* parse_layout = new QVBoxLayout(parse_container);
    parse_layout->setContentsMargins(6, 6, 6, 6);
    parse_state_ = new QLabel("Combat logging is unavailable");
    parse_state_->setObjectName("parse-state");
    parse_state_->setWordWrap(true);
    parse_layout->addWidget(parse_state_);
    parse_table_ = new QTableWidget;
    parse_table_->setObjectName("parse-table");
    parse_table_->setColumnCount(5);
    parse_table_->setHorizontalHeaderLabels(
        {"Participant", "Damage", "DPS", "%", "Active"});
    parse_table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    parse_table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    parse_table_->setSelectionMode(QAbstractItemView::SingleSelection);
    parse_table_->setAlternatingRowColors(true);
    parse_table_->verticalHeader()->setVisible(false);
    parse_table_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column) {
        parse_table_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    parse_layout->addWidget(parse_table_, 1);
    auto* parse_dock = create_dock(
        "Parse", "parse-dock", parse_container, this);
    addDockWidget(Qt::LeftDockWidgetArea, parse_dock);
    splitDockWidget(character_dock, parse_dock, Qt::Vertical);

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
    spawn_type_filter_->addItem("Other", 2);
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

    build_menu_bar(character_dock, parse_dock, spawn_dock, detail_dock);

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

void MainWindow::build_menu_bar(QDockWidget* character_dock,
                                QDockWidget* parse_dock,
                                QDockWidget* spawn_dock,
                                QDockWidget* detail_dock) {
    QMenuBar* bar = menuBar();
    bar->setObjectName("main-menu-bar");
    bar->setNativeMenuBar(false);

    QMenu* views_menu = bar->addMenu("&Views");
    views_menu->setObjectName("views-menu");
    const std::array<std::pair<QDockWidget*, QString>, 4> views{
        std::pair{character_dock, QString("view-character-action")},
        std::pair{parse_dock, QString("view-parse-action")},
        std::pair{spawn_dock, QString("view-spawns-action")},
        std::pair{detail_dock, QString("view-details-action")},
    };
    for (const auto& [dock, object_name] : views) {
        QAction* action = dock->toggleViewAction();
        action->setObjectName(object_name);
        views_menu->addAction(action);
    }

    auto* window_controls = new QWidget(bar);
    window_controls->setObjectName("window-controls");
    auto* controls_layout = new QHBoxLayout(window_controls);
    controls_layout->setContentsMargins(0, 0, 0, 0);
    controls_layout->setSpacing(0);

    const auto add_control = [this, controls_layout](
                                 const QString& object_name,
                                 const QString& tooltip,
                                 QStyle::StandardPixmap icon) {
        auto* button = new QToolButton;
        button->setObjectName(object_name);
        button->setAccessibleName(tooltip);
        button->setToolTip(tooltip);
        button->setAutoRaise(true);
        button->setFocusPolicy(Qt::StrongFocus);
        button->setIcon(style()->standardIcon(icon));
        const int extent = style()->pixelMetric(QStyle::PM_SmallIconSize) + 8;
        button->setFixedSize(extent, extent);
        controls_layout->addWidget(button);
        return button;
    };

    QToolButton* minimize_button = add_control(
        "window-minimize-button", "Minimize",
        QStyle::SP_TitleBarMinButton);
    maximize_button_ = add_control(
        "window-maximize-button", "Maximize",
        QStyle::SP_TitleBarMaxButton);
    QToolButton* close_button = add_control(
        "window-close-button", "Close",
        QStyle::SP_TitleBarCloseButton);

    connect(minimize_button, &QToolButton::clicked,
            this, &QWidget::showMinimized);
    connect(maximize_button_, &QToolButton::clicked, this, [this]() {
        if (isMaximized()) {
            showNormal();
        } else {
            showMaximized();
        }
    });
    connect(close_button, &QToolButton::clicked, this, &QWidget::close);
    bar->setCornerWidget(window_controls, Qt::TopRightCorner);
    update_maximize_button();
}

void MainWindow::update_maximize_button() {
    if (maximize_button_ == nullptr) {
        return;
    }
    const bool maximized = isMaximized();
    const QString label = maximized ? "Restore" : "Maximize";
    maximize_button_->setAccessibleName(label);
    maximize_button_->setToolTip(label);
    maximize_button_->setIcon(style()->standardIcon(
        maximized ? QStyle::SP_TitleBarNormalButton
                  : QStyle::SP_TitleBarMaxButton));
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

void MainWindow::update_character_snapshot(
    const CharacterSnapshot& snapshot) {
    equipment_table_->setRowCount(0);
    if (!snapshot.available()) {
        character_name_->setText(
            QString::fromStdString(snapshot.detail));
        set_vital_unavailable(health_bar_, "HP");
        set_vital_unavailable(mana_bar_, "Mana");
        return;
    }
    character_name_->setText(QString::fromStdString(snapshot.name));
    update_vital_bar(health_bar_, "HP", snapshot.health);
    update_vital_bar(mana_bar_, "Mana", snapshot.mana);
    equipment_table_->setRowCount(
        static_cast<int>(snapshot.equipment.size()));
    for (std::size_t row = 0U; row < snapshot.equipment.size(); ++row) {
        const EquipmentSlotSnapshot& equipment = snapshot.equipment[row];
        equipment_table_->setItem(
            static_cast<int>(row), 0,
            new QTableWidgetItem(QString::fromStdString(equipment.slot)));
        equipment_table_->setItem(
            static_cast<int>(row), 1,
            new QTableWidgetItem(
                equipment.item.empty()
                    ? QString::fromLatin1("Empty")
                    : QString::fromStdString(equipment.item)));
    }
}

void MainWindow::update_combat_snapshot(
    const CombatEncounterSnapshot& snapshot) {
    const double current_dps =
        snapshot.state == CombatEncounterState::active
            ? snapshot.active_character_dps
            : 0.0;
    current_dps_->setText(
        QString("Current DPS: %1")
            .arg(current_dps, 0, 'f', 1));
    parse_table_->setRowCount(
        static_cast<int>(snapshot.participants.size()));
    for (std::size_t row = 0U; row < snapshot.participants.size(); ++row) {
        const CombatParticipantSnapshot& participant =
            snapshot.participants[row];
        const int table_row = static_cast<int>(row);
        parse_table_->setItem(
            table_row, 0,
            new QTableWidgetItem(QString::fromStdString(participant.name)));
        parse_table_->setItem(
            table_row, 1,
            new QTableWidgetItem(QString::number(participant.damage)));
        parse_table_->setItem(
            table_row, 2,
            new QTableWidgetItem(
                QString::number(participant.dps, 'f', 1)));
        parse_table_->setItem(
            table_row, 3,
            new QTableWidgetItem(
                QString::number(participant.percentage, 'f', 1)));
        parse_table_->setItem(
            table_row, 4,
            new QTableWidgetItem(
                QString::number(participant.active_seconds, 'f', 1)));
    }
    if (!snapshot.available()) {
        parse_state_->setText(QString::fromStdString(snapshot.detail));
        return;
    }
    parse_state_->setText(
        QString("%1 - %2 - %3 damage - %4 s")
            .arg(QString::fromStdString(snapshot.detail))
            .arg(QString::fromStdString(snapshot.target))
            .arg(snapshot.total_damage)
            .arg(snapshot.duration_seconds, 0, 'f', 1));
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
                "<p>Level %2 %3</p>"
                "<p>X %4 - Y %5 - Z %6 - Distance %7</p>")
            .arg(name)
            .arg(spawn->level)
            .arg(QString::fromLatin1(spawn_presentation_label(*spawn)))
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
    if (const auto state = settings_.load()) {
        if (client_directory_.isEmpty()) {
            client_directory_ = state->client_directory;
        }
        if (!reset_layout_) {
            bool layout_restored = false;
            bool geometry_restored = false;
            if (!state->layout.isEmpty() &&
                !state->geometry.isEmpty()) {
                layout_restored = restoreState(state->layout);
                geometry_restored =
                    restoreGeometry(state->geometry);
            }
            if (!geometry_restored || !layout_restored) {
                resize(1200, 780);
            }
        }
        map_canvas_->set_height_filter_range(
            state->height_filter_below,
            state->height_filter_above);
        map_canvas_->set_height_filter_enabled(
            state->height_filter_enabled);
        map_canvas_->set_player_follow_enabled(
            state->player_follow_enabled);
        map_canvas_->set_named_spawn_labels_visible(
            state->named_spawn_labels_visible);
        map_canvas_->set_player_labels_visible(
            state->player_labels_visible);
        map_canvas_->set_npc_labels_visible(
            state->npc_labels_visible);
        map_canvas_->set_named_spawns_visible(
            state->named_spawns_visible);
        map_canvas_->set_player_spawns_visible(
            state->player_spawns_visible);
        map_canvas_->set_npc_spawns_visible(
            state->npc_spawns_visible);
        map_canvas_->set_other_spawns_visible(
            state->other_spawns_visible);
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
    }
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
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
        .client_directory = client_directory_,
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
        .named_spawn_labels_visible =
            map_canvas_->named_spawn_labels_visible(),
        .player_labels_visible =
            map_canvas_->player_labels_visible(),
        .npc_labels_visible =
            map_canvas_->npc_labels_visible(),
        .named_spawns_visible =
            map_canvas_->named_spawns_visible(),
        .player_spawns_visible =
            map_canvas_->player_spawns_visible(),
        .npc_spawns_visible =
            map_canvas_->npc_spawns_visible(),
        .other_spawns_visible =
            map_canvas_->other_spawns_visible(),
        .spawn_filter = spawn_filter_->text(),
        .spawn_type_filter = type_filter,
        .spawn_sort_column = header->sortIndicatorSection(),
        .spawn_sort_descending =
            header->sortIndicatorOrder() == Qt::DescendingOrder,
        .spawn_column_widths = column_widths,
    };
    (void)settings_.save(state);
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        update_maximize_button();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    save_ui_state();
    QMainWindow::closeEvent(event);
    QCoreApplication::quit();
}

}  // namespace plazmic
