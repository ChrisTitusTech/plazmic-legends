#include "ui/main_window.h"

#include "activity/activity_tracker.h"
#include "game/combat_history_store.h"

#include "ui/character_profile_exporter.h"
#include "ui/map_canvas.h"
#include "ui/spawn_presentation.h"
#include "ui/spawn_table_model.h"
#include "ui/ui_file_installer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <ranges>
#include <utility>

#include <QAbstractItemView>
#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QCheckBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDockWidget>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTableView>
#include <QTabWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace plazmic {
namespace {

constexpr std::size_t kMaximumCombatAbilityRows =
    CombatHistoryStore::maximum_total_abilities;

QString activity_kind_label(ActivityEventKind kind) {
    switch (kind) {
        case ActivityEventKind::experience:
            return "XP";
        case ActivityEventKind::alternate_advancement:
            return "AA";
        case ActivityEventKind::loot:
            return "Loot";
        case ActivityEventKind::equipment_change:
            return "Equipment";
        case ActivityEventKind::celebration:
            return "Celebration";
    }
    return "Activity";
}

QString experience_summary(
    const CharacterSnapshot& character,
    const ActivityAnalyticsSnapshot& analytics) {
    const QString current = character.experience_percent
                                ? QString::number(
                                      *character.experience_percent, 'f', 3) +
                                      "%"
                                : QString("unavailable");
    if (!analytics.available) {
        return QString("XP: %1 | gain rate: unavailable").arg(current);
    }
    const QString level_pace = analytics.level_pace_hours
                                   ? QString::number(
                                         *analytics.level_pace_hours,
                                         'f',
                                         1) +
                                         "h/100%"
                                   : QString("collecting");
    return QString("XP: %1 | gain rate %2%/h | pace %3")
        .arg(current)
        .arg(analytics.experience_percent_per_hour, 0, 'f', 3)
        .arg(level_pace);
}

bool same_inventory_context(const CharacterSnapshot& left,
                            const CharacterSnapshot& right) {
    return left.state == right.state && left.name == right.name &&
           left.equipment == right.equipment;
}

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

void set_vital_format(QProgressBar* bar, const QString& format) {
    constexpr int kTextPadding = 32;
    bar->setFormat(format);
    bar->setToolTip(format);
    bar->setMinimumWidth(
        bar->fontMetrics().horizontalAdvance(format) + kTextPadding);
}

void set_vital_unavailable(QProgressBar* bar, const QString& label) {
    bar->setRange(0, 100);
    bar->setValue(0);
    set_vital_format(bar, QString("%1 unavailable").arg(label));
}

void update_vital_bar(QProgressBar* bar,
                      const QString& label,
                      const VitalSnapshot& vital) {
    if (!vital.maximum) {
        set_vital_unavailable(bar, label);
        return;
    }
    if (*vital.maximum == 0) {
        bar->setRange(0, 100);
        bar->setValue(0);
        set_vital_format(bar, QString("%1 0 / 0 (0%)").arg(label));
        return;
    }
    const std::int64_t maximum = *vital.maximum;
    const double ratio = vital.percentage().value_or(0.0);
    const int percentage = std::clamp(
        static_cast<int>(std::lround(ratio)), 0, 100);
    bar->setRange(0, static_cast<int>(maximum));
    bar->setValue(static_cast<int>(
        std::clamp(vital.current, std::int64_t{0}, maximum)));
    set_vital_format(
        bar,
        QString("%1 %2 / %3 (%4%)")
            .arg(label)
            .arg(vital.current)
            .arg(maximum)
            .arg(percentage));
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
    health_bar_->setStyleSheet(
        "QProgressBar { text-align: center; } "
        "QProgressBar::chunk { background-color: #d32f2f; }");
    character_layout->addWidget(health_bar_);
    mana_bar_ = new QProgressBar;
    mana_bar_->setObjectName("character-mana");
    mana_bar_->setTextVisible(true);
    mana_bar_->setStyleSheet(
        "QProgressBar { text-align: center; } "
        "QProgressBar::chunk { background-color: #1976d2; }");
    character_layout->addWidget(mana_bar_);
    character_layout->addStretch(1);
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
    combat_overview_ = new QLabel("No retained combat history");
    combat_overview_->setObjectName("combat-overview");
    combat_overview_->setWordWrap(true);
    parse_layout->addWidget(combat_overview_);
    auto* combat_tabs = new QTabWidget;
    combat_tabs->setObjectName("combat-tabs");
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
    combat_tabs->addTab(parse_table_, "Damage");

    combat_abilities_ = new QTableWidget;
    combat_abilities_->setObjectName("combat-abilities");
    combat_abilities_->setColumnCount(5);
    combat_abilities_->setHorizontalHeaderLabels(
        {"Participant", "Attack / Spell", "Type", "Damage", "Hits"});
    combat_abilities_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    combat_abilities_->setAlternatingRowColors(true);
    combat_abilities_->verticalHeader()->setVisible(false);
    combat_abilities_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    combat_abilities_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    for (int column = 2; column < 5; ++column) {
        combat_abilities_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    combat_tabs->addTab(combat_abilities_, "Attacks / Spells");

    combat_healing_ = new QTableWidget;
    combat_healing_->setObjectName("combat-healing");
    combat_healing_->setColumnCount(5);
    combat_healing_->setHorizontalHeaderLabels(
        {"Healer", "Healing", "Events", "HPS", "%"});
    combat_healing_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    combat_healing_->setAlternatingRowColors(true);
    combat_healing_->verticalHeader()->setVisible(false);
    combat_healing_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    for (int column = 1; column < 5; ++column) {
        combat_healing_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    combat_tabs->addTab(combat_healing_, "Healing");

    combat_timeline_ = new QTableWidget;
    combat_timeline_->setObjectName("combat-timeline");
    combat_timeline_->setColumnCount(3);
    combat_timeline_->setHorizontalHeaderLabels(
        {"Second", "Damage", "Healing"});
    combat_timeline_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    combat_timeline_->setAlternatingRowColors(true);
    combat_timeline_->verticalHeader()->setVisible(false);
    combat_timeline_->horizontalHeader()->setSectionResizeMode(
        QHeaderView::Stretch);
    combat_tabs->addTab(combat_timeline_, "Timeline");

    combat_history_ = new QTableWidget;
    combat_history_->setObjectName("combat-history");
    combat_history_->setColumnCount(5);
    combat_history_->setHorizontalHeaderLabels(
        {"Zone", "Target", "Damage", "Healing", "Seconds"});
    combat_history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    combat_history_->setSelectionBehavior(QAbstractItemView::SelectRows);
    combat_history_->setSelectionMode(QAbstractItemView::SingleSelection);
    combat_history_->setAlternatingRowColors(true);
    combat_history_->verticalHeader()->setVisible(false);
    combat_history_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    combat_history_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    for (int column = 2; column < 5; ++column) {
        combat_history_->horizontalHeader()->setSectionResizeMode(
            column, QHeaderView::ResizeToContents);
    }
    combat_tabs->addTab(combat_history_, "History");
    connect(combat_history_, &QTableWidget::itemSelectionChanged,
            this, [this]() {
                if (updating_combat_history_) {
                    return;
                }
                const int row = combat_history_->currentRow();
                if (row < 0 ||
                    !combat_history_->selectionModel()->isRowSelected(
                        row, QModelIndex()) ||
                    static_cast<std::size_t>(row) >=
                        combat_analytics_.history.size()) {
                    selected_combat_history_.reset();
                    render_combat_encounter(combat_analytics_.encounter);
                    return;
                }
                const std::size_t index =
                    combat_analytics_.history.size() -
                    static_cast<std::size_t>(row) - 1U;
                selected_combat_history_ =
                    combat_analytics_.history[index].started_unix_seconds;
                render_combat_encounter(combat_analytics_.history[index]);
                if (!combat_analytics_.encounter.available()) {
                    parse_state_->setText(QString::fromStdString(
                        combat_analytics_.encounter.detail));
                }
            });
    parse_layout->addWidget(combat_tabs, 1);
    auto* parse_dock = create_dock(
        "Parse", "parse-dock", parse_container, this);
    addDockWidget(Qt::LeftDockWidgetArea, parse_dock);
    splitDockWidget(character_dock, parse_dock, Qt::Vertical);

    auto* activity_container = new QWidget;
    auto* activity_layout = new QVBoxLayout(activity_container);
    activity_layout->setContentsMargins(6, 6, 6, 6);
    activity_state_ = new QLabel;
    activity_state_->setObjectName("activity-state");
    activity_state_->setTextFormat(Qt::PlainText);
    activity_state_->setWordWrap(true);
    activity_state_->hide();
    activity_layout->addWidget(activity_state_);
    auto* activity_tabs = new QTabWidget;
    activity_tabs->setObjectName("activity-tabs");

    activity_events_ = new QTableWidget;
    activity_events_->setObjectName("activity-events");
    activity_events_->setColumnCount(4);
    activity_events_->setHorizontalHeaderLabels(
        {"Type", "Zone", "Activity", "Evidence"});
    activity_events_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activity_events_->setAlternatingRowColors(true);
    activity_events_->verticalHeader()->setVisible(false);
    activity_events_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    activity_events_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    activity_events_->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch);
    activity_events_->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch);
    activity_tabs->addTab(activity_events_, "Progression / Loot");

    activity_abilities_ = new QTableWidget;
    activity_abilities_->setObjectName("activity-abilities");
    activity_abilities_->setColumnCount(5);
    activity_abilities_->setHorizontalHeaderLabels(
        {"Ability", "Type", "Damage", "Observations", "Confidence"});
    activity_abilities_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activity_abilities_->setAlternatingRowColors(true);
    activity_abilities_->verticalHeader()->setVisible(false);
    activity_abilities_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    activity_abilities_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents);
    activity_abilities_->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    activity_abilities_->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::ResizeToContents);
    activity_abilities_->horizontalHeader()->setSectionResizeMode(
        4, QHeaderView::Stretch);
    activity_tabs->addTab(activity_abilities_, "Class / Proc Evidence");

    auto* inventory_container = new QWidget;
    inventory_container->setObjectName("activity-inventory");
    auto* inventory_layout = new QVBoxLayout(inventory_container);
    inventory_layout->setContentsMargins(0, 0, 0, 0);
    auto* equipment_heading = new QLabel("Equipped Items");
    equipment_heading->setObjectName("inventory-equipment-heading");
    inventory_layout->addWidget(equipment_heading);
    equipment_state_ = new QLabel("Character information unavailable");
    equipment_state_->setObjectName("inventory-equipment-state");
    equipment_state_->setTextFormat(Qt::PlainText);
    equipment_state_->setWordWrap(true);
    inventory_layout->addWidget(equipment_state_);
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
    inventory_layout->addWidget(equipment_table_, 1);
    auto* reconciliation_heading = new QLabel("Imported Inventory Output");
    reconciliation_heading->setObjectName(
        "inventory-reconciliation-heading");
    inventory_layout->addWidget(reconciliation_heading);
    inventory_state_ = new QLabel(
        "Select User > Import Inventory Output to reconcile a local file");
    inventory_state_->setObjectName("inventory-reconciliation-state");
    inventory_state_->setTextFormat(Qt::PlainText);
    inventory_state_->setWordWrap(true);
    inventory_layout->addWidget(inventory_state_);
    inventory_reconciliation_ = new QTableWidget;
    inventory_reconciliation_->setObjectName("inventory-reconciliation");
    inventory_reconciliation_->setColumnCount(3);
    inventory_reconciliation_->setHorizontalHeaderLabels(
        {"Location", "Item", "Quantity"});
    inventory_reconciliation_->setEditTriggers(
        QAbstractItemView::NoEditTriggers);
    inventory_reconciliation_->setAlternatingRowColors(true);
    inventory_reconciliation_->verticalHeader()->setVisible(false);
    inventory_reconciliation_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents);
    inventory_reconciliation_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    inventory_reconciliation_->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::ResizeToContents);
    inventory_layout->addWidget(inventory_reconciliation_, 1);
    activity_tabs->addTab(inventory_container, "Inventory");
    activity_layout->addWidget(activity_tabs, 1);
    auto* activity_dock = create_dock(
        "Activity", "activity-dock", activity_container, this);
    addDockWidget(Qt::LeftDockWidgetArea, activity_dock);
    tabifyDockWidget(parse_dock, activity_dock);
    parse_dock->raise();

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

    auto* details_container = new QWidget;
    details_container->setObjectName("details-content");
    auto* details_layout = new QVBoxLayout(details_container);
    details_layout->setContentsMargins(0, 0, 0, 0);
    details_layout->setSpacing(0);

    auto* activity_summary_bar = new QWidget;
    activity_summary_bar->setObjectName("activity-summary-bar");
    activity_summary_bar->setStyleSheet(
        "#activity-summary-bar { border-bottom: 1px solid palette(mid); }");
    auto* activity_summary_layout = new QHBoxLayout(activity_summary_bar);
    activity_summary_layout->setContentsMargins(0, 4, 0, 4);
    activity_summary_layout->setSpacing(0);
    activity_summary_splitter_ =
        new QSplitter(Qt::Horizontal, activity_summary_bar);
    activity_summary_splitter_->setObjectName("activity-summary-splitter");
    activity_summary_splitter_->setChildrenCollapsible(false);
    activity_summary_splitter_->setHandleWidth(6);
    current_dps_ = new QLabel("DPS: 0.0");
    current_dps_->setObjectName("activity-summary-dps");
    activity_xp_summary_ = new QLabel("XP: waiting");
    activity_xp_summary_->setObjectName("activity-summary-xp");
    activity_aa_summary_ = new QLabel("AA: waiting");
    activity_aa_summary_->setObjectName("activity-summary-aa");
    activity_latest_summary_ = new QLabel("Latest: waiting");
    activity_latest_summary_->setObjectName("activity-summary-latest");
    const std::array<QLabel*, 4> summary_labels{
        current_dps_, activity_xp_summary_, activity_aa_summary_,
        activity_latest_summary_};
    for (QLabel* label : summary_labels) {
        label->setTextFormat(Qt::PlainText);
        label->setMinimumWidth(0);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setContentsMargins(8, 0, 8, 0);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        activity_summary_splitter_->addWidget(label);
    }
    activity_summary_splitter_->setStretchFactor(0, 1);
    activity_summary_splitter_->setStretchFactor(1, 2);
    activity_summary_splitter_->setStretchFactor(2, 3);
    activity_summary_splitter_->setStretchFactor(3, 3);
    activity_summary_splitter_->setSizes({100, 240, 300, 260});
    connect(
        activity_summary_splitter_, &QSplitter::splitterMoved, this,
        [this]() {
            const QList<int> sizes = activity_summary_splitter_->sizes();
            if (sizes.size() == 4) {
                activity_summary_widths_ =
                    {sizes[0], sizes[1], sizes[2], sizes[3]};
            }
        });
    activity_summary_layout->addWidget(activity_summary_splitter_);
    details_layout->addWidget(activity_summary_bar);

    selection_detail_ = new QLabel("No spawn selected.");
    selection_detail_->setObjectName("selection-detail");
    selection_detail_->setAlignment(Qt::AlignCenter);
    selection_detail_->setWordWrap(true);
    details_layout->addWidget(selection_detail_, 1);
    auto* detail_dock = create_dock(
        "Details", "detail-dock", details_container, this);
    addDockWidget(Qt::BottomDockWidgetArea, detail_dock);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);

    build_menu_bar(character_dock, parse_dock, activity_dock,
                   spawn_dock, detail_dock);

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

void MainWindow::open_inventory_export() {
    const CharacterSnapshot snapshot_for_export = character_snapshot_;
    const CharacterProfileExport profile =
        build_character_profile_export(snapshot_for_export);
    if (!profile) {
        QMessageBox::warning(this, "Export Inventory", profile.detail);
        return;
    }

    const QString destination = QFileDialog::getSaveFileName(
        this, "Export Inventory for EQ Legends Tools",
        QDir::home().filePath(profile.suggested_file_name),
        "JSON profile backup (*.json)");
    if (destination.isEmpty()) {
        return;
    }
    if (!character_profile_export_is_current(snapshot_for_export,
                                             character_snapshot_)) {
        QMessageBox::warning(
            this, "Export Inventory",
            "Character information changed while selecting the export file. "
            "Open Export Inventory again with a current snapshot.");
        return;
    }
    const CharacterProfileSaveResult result =
        save_character_profile_export(destination, character_snapshot_);
    if (!result) {
        QMessageBox::critical(this, "Export Inventory", result.detail);
        return;
    }
    QMessageBox::information(
        this, "Export Inventory",
        QString(
            "Saved %1 equipped item(s).\n\n"
            "At eqlegendstools.com/char-sheet/, choose Import Profile "
            "Backup and select the JSON file. Verify race, tri-class, "
            "favored stats, Alternate Advancement, upgrades, and "
            "Exaltations after import.")
            .arg(result.equipped_items));
}

void MainWindow::open_inventory_import() {
    const QString source = QFileDialog::getOpenFileName(
        this, "Import EverQuest Inventory Output", QDir::homePath(),
        "EverQuest inventory output (*.txt);;All files (*)");
    if (source.isEmpty()) {
        return;
    }
    const InventoryReconciliationSnapshot imported =
        import_inventory_output(
            std::filesystem::path(source.toStdString()),
            character_snapshot_);
    if (!imported.available) {
        update_inventory_reconciliation(imported, {});
        QMessageBox::warning(
            this, "Import Inventory Output",
            QString::fromStdString(imported.detail));
        return;
    }
    update_inventory_reconciliation(imported, source);
}

void MainWindow::update_inventory_reconciliation(
    InventoryReconciliationSnapshot snapshot,
    QString source_path) {
    if (!snapshot.available || source_path.isEmpty()) {
        snapshot.entries.clear();
        snapshot.equipped_not_in_import.clear();
        snapshot.imported_equipped_items.clear();
        snapshot.source_name.clear();
        snapshot.available = false;
        if (snapshot.detail.empty()) {
            snapshot.detail = "Inventory import is unavailable";
        }
        inventory_path_.clear();
        inventory_character_name_.clear();
        render_inventory_reconciliation(snapshot);
        return;
    }
    inventory_path_ = std::move(source_path);
    inventory_character_name_ = character_snapshot_.available()
                                    ? character_snapshot_.name
                                    : std::string{};
    render_inventory_reconciliation(snapshot);
}

void MainWindow::export_activity_history() {
    if (activity_analytics_.events.empty() &&
        activity_analytics_.abilities.empty()) {
        QMessageBox::information(
            this, "Export Activity History",
            "No progression, loot, equipment, or ability observations are "
            "available to export.");
        return;
    }
    const ActivityAnalyticsSnapshot snapshot_for_export = activity_analytics_;
    const QString destination = QFileDialog::getSaveFileName(
        this, "Export Activity History",
        QDir::home().filePath("plazmic-activity.json"),
        "Plazmic activity JSON (*.json)");
    if (destination.isEmpty()) {
        return;
    }
    if (!same_activity_export_payload(
            snapshot_for_export, activity_analytics_)) {
        QMessageBox::warning(
            this, "Export Activity History",
            "The displayed activity changed while choosing the export file. "
            "Open Export Activity History again.");
        return;
    }
    if (!save_activity_export(
            std::filesystem::path(destination.toStdString()),
            snapshot_for_export)) {
        QMessageBox::critical(
            this, "Export Activity History",
            "The bounded owner-only activity export could not be saved.");
        return;
    }
    QMessageBox::information(
        this, "Export Activity History",
        "Saved the currently displayed local activity observations.");
}

void MainWindow::delete_activity_history() {
    const std::string selected_key = activity_analytics_.storage_key;
    if (selected_key.empty()) {
        return;
    }
    if (QMessageBox::question(
            this, "Delete Activity History",
            "Delete the selected character's retained progression, loot, "
            "equipment, and ability observations? Retain Activity History "
            "is also turned off. This cannot be undone.",
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) !=
        QMessageBox::Yes) {
        return;
    }
    (void)queue_activity_history_deletion(selected_key);
}

bool MainWindow::queue_activity_history_deletion(
    std::string_view confirmed_key) {
    if (activity_analytics_.storage_key != confirmed_key) {
        statusBar()->showMessage(
            "The selected activity partition changed while confirming; open "
            "Delete Activity History again");
        return false;
    }
    if (!delete_activity_callback_) {
        statusBar()->showMessage(
            "Activity history deletion is unavailable; retention was not "
            "changed");
        return false;
    }
    retain_activity_history_action_->setChecked(false);
    if (retain_activity_history_action_->isChecked()) {
        statusBar()->showMessage(
            "Activity retention could not be turned off safely; deletion was "
            "not queued");
        return false;
    }
    delete_activity_callback_(std::string(confirmed_key));
    delete_activity_action_->setEnabled(false);
    return true;
}

void MainWindow::report_activity_deletion_result(std::string_view key,
                                                 bool succeeded) {
    if (succeeded) {
        if (activity_analytics_.storage_key == key) {
            statusBar()->clearMessage();
            auto deleted = ActivityAnalyticsSnapshot{};
            deleted.detail = "Activity history deleted";
            update_activity_snapshot(deleted);
        }
        return;
    }
    const QString detail =
        "Activity history deletion failed; the retained file remains on disk "
        "and can be retried";
    if (activity_analytics_.storage_key == key) {
        statusBar()->showMessage(detail);
        activity_analytics_.persisted = false;
        activity_analytics_.detail = detail.toStdString();
        update_activity_snapshot(activity_analytics_);
    }
}

void MainWindow::build_menu_bar(QDockWidget* character_dock,
                                QDockWidget* parse_dock,
                                QDockWidget* activity_dock,
                                QDockWidget* spawn_dock,
                                QDockWidget* detail_dock) {
    QMenuBar* bar = menuBar();
    bar->setObjectName("main-menu-bar");
    bar->setNativeMenuBar(false);

    QMenu* user_menu = bar->addMenu("&User");
    user_menu->setObjectName("user-menu");
    export_inventory_action_ = user_menu->addAction("Export Inventory...");
    export_inventory_action_->setObjectName("export-inventory-action");
    export_inventory_action_->setEnabled(false);
    connect(export_inventory_action_, &QAction::triggered,
            this, &MainWindow::open_inventory_export);
    import_inventory_action_ =
        user_menu->addAction("Import Inventory Output...");
    import_inventory_action_->setObjectName("import-inventory-action");
    connect(import_inventory_action_, &QAction::triggered,
            this, &MainWindow::open_inventory_import);
    QAction* ui_install_action =
        user_menu->addAction("UI File Install...");
    ui_install_action->setObjectName("ui-file-install-action");
    connect(ui_install_action, &QAction::triggered,
            this, &MainWindow::open_ui_file_install);
    retain_combat_history_action_ =
        user_menu->addAction("Retain Combat History");
    retain_combat_history_action_->setObjectName(
        "retain-combat-history-action");
    retain_combat_history_action_->setCheckable(true);
    connect(retain_combat_history_action_, &QAction::toggled,
            this, [this]() { save_combat_history_preference(); });
    retain_activity_history_action_ =
        user_menu->addAction("Retain Activity History");
    retain_activity_history_action_->setObjectName(
        "retain-activity-history-action");
    retain_activity_history_action_->setCheckable(true);
    connect(retain_activity_history_action_, &QAction::toggled,
            this, [this]() { save_activity_history_preference(); });
    export_activity_action_ =
        user_menu->addAction("Export Activity History...");
    export_activity_action_->setObjectName("export-activity-action");
    export_activity_action_->setEnabled(false);
    connect(export_activity_action_, &QAction::triggered,
            this, &MainWindow::export_activity_history);
    delete_activity_action_ =
        user_menu->addAction("Delete Activity History...");
    delete_activity_action_->setObjectName("delete-activity-action");
    delete_activity_action_->setEnabled(false);
    connect(delete_activity_action_, &QAction::triggered,
            this, &MainWindow::delete_activity_history);

    QMenu* views_menu = bar->addMenu("&Views");
    views_menu->setObjectName("views-menu");
    const std::array<std::pair<QDockWidget*, QString>, 5> views{
        std::pair{character_dock, QString("view-character-action")},
        std::pair{parse_dock, QString("view-parse-action")},
        std::pair{activity_dock, QString("view-activity-action")},
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

void MainWindow::open_ui_file_install() {
    if (client_directory_.isEmpty()) {
        QMessageBox::warning(
            this, "UI File Install",
            "Configure a valid EverQuest Legends directory first.");
        return;
    }
    const QString bundle_directory = QFileDialog::getExistingDirectory(
        this, "Select extracted private Plazmic UI bundle");
    if (bundle_directory.isEmpty()) {
        return;
    }
    const UiBundleInspection bundle = inspect_ui_bundle(bundle_directory);
    if (!bundle.bundle) {
        QMessageBox::critical(this, "UI File Install", bundle.detail);
        return;
    }
    const UiInstallTargetInspection target =
        inspect_ui_install_targets(client_directory_);
    if (!target.targets) {
        QMessageBox::critical(this, "UI File Install", target.detail);
        return;
    }

    QDialog dialog(this);
    dialog.setWindowTitle("UI File Install");
    dialog.setObjectName("ui-file-install-dialog");
    auto* layout = new QFormLayout(&dialog);
    auto* description = new QLabel(
        QString("Install private %1 settings. Existing files are backed up "
                "before replacement. A separate UI_plazmic_1440p.ini copy "
                "is installed so a running client can import the layout.")
            .arg(bundle.bundle->resolution));
    description->setWordWrap(true);
    layout->addRow(description);

    const auto add_paths = [&dialog](const QStringList& paths,
                                     const QString& object_name) {
        auto* combo = new QComboBox(&dialog);
        combo->setObjectName(object_name);
        for (const QString& path : paths) {
            combo->addItem(QFileInfo(path).fileName(), path);
        }
        return combo;
    };
    QComboBox* source_layout = add_paths(
        bundle.bundle->layout_inis, "ui-install-source-layout");
    const int cohesive_layout =
        source_layout->findText("UI_plazmic_1440p.ini");
    if (cohesive_layout >= 0) {
        source_layout->setCurrentIndex(cohesive_layout);
        source_layout->setItemText(
            cohesive_layout, "UI_plazmic_1440p.ini (Recommended)");
    }
    QComboBox* target_layout = add_paths(
        target.targets->layout_inis, "ui-install-target-layout");
    QComboBox* source_character = add_paths(
        bundle.bundle->character_inis, "ui-install-source-character");
    QComboBox* target_character = add_paths(
        target.targets->character_inis, "ui-install-target-character");
    layout->addRow("Source window layout:", source_layout);
    layout->addRow("Write layout into:", target_layout);
    layout->addRow("Source character filters:", source_character);
    layout->addRow("Write character settings into:", target_character);

    auto* global_settings = new QCheckBox(
        "Install bundled eqclient.ini global filters and 1440p settings",
        &dialog);
    global_settings->setObjectName("ui-install-global-settings");
    global_settings->setChecked(true);
    layout->addRow(global_settings);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText("Install");
    connect(buttons, &QDialogButtonBox::accepted,
            &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected,
            &dialog, &QDialog::reject);
    layout->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString confirmation =
        QString("Replace %1 and %2%3, create the live layout source, and "
                "install the Plazmic UI skin?\n\n"
                "A private rollback directory will be created first.")
            .arg(target_layout->currentText())
            .arg(target_character->currentText())
            .arg(global_settings->isChecked() ? " plus eqclient.ini" : "");
    if (QMessageBox::question(
            this, "Confirm UI File Install", confirmation,
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) !=
        QMessageBox::Yes) {
        return;
    }

    const UiFileInstallResult result = install_ui_bundle({
        .bundle_directory = bundle_directory,
        .game_directory = client_directory_,
        .source_layout_ini = source_layout->currentData().toString(),
        .target_layout_ini = target_layout->currentData().toString(),
        .source_character_ini = source_character->currentData().toString(),
        .target_character_ini = target_character->currentData().toString(),
        .install_global_ini = global_settings->isChecked(),
    });
    if (!result.installed) {
        QMessageBox::critical(
            this, "UI File Install",
            result.backup_directory.isEmpty()
                ? result.detail
                : result.detail + "\n\nRollback: " +
                      result.backup_directory);
        return;
    }
    const QString live_layout_name = "UI_plazmic_1440p.ini";
    QMessageBox::information(
        this, "UI File Install",
        result.detail + "\n\nRollback: " + result.backup_directory +
            "\n\nApply it in the running game:\n"
            "1. Enter /copylayout.\n"
            "2. Select " + live_layout_name +
            " (shown as plazmic on 1440p in some clients), then copy the "
            "window layout.\n"
            "3. After the windows move, enter /loadskin plazmic-ui 1.\n\n"
            "If /copylayout was already open during installation, close and "
            "reopen it so the new source appears. The UI can disappear "
            "briefly while /loadskin finishes.");
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
    const bool inventory_character_changed =
        !inventory_path_.isEmpty() &&
        !inventory_character_name_.empty() &&
        snapshot.available() && snapshot.name != inventory_character_name_;
    const bool inventory_character_lost =
        !inventory_path_.isEmpty() &&
        !inventory_character_name_.empty() && !snapshot.available();
    const bool inventory_character_arrived =
        !inventory_path_.isEmpty() && inventory_character_name_.empty() &&
        snapshot.available();
    const bool inventory_reconcile_changed =
        !inventory_path_.isEmpty() &&
        !inventory_character_changed && !inventory_character_lost &&
        (inventory_character_arrived ||
         (!inventory_character_name_.empty() &&
          !same_inventory_context(snapshot, character_snapshot_)));
    character_snapshot_ = snapshot;
    const QString xp_summary =
        experience_summary(character_snapshot_, activity_analytics_);
    activity_xp_summary_->setText(xp_summary);
    activity_xp_summary_->setToolTip(xp_summary);
    if (inventory_character_changed || inventory_character_lost) {
        inventory_character_name_.clear();
        render_inventory_reconciliation({
            .entries = {},
            .equipped_not_in_import = {},
            .imported_equipped_items = {},
            .source_name = {},
            .detail = inventory_character_lost
                          ? "Inventory import cleared after the active "
                            "character became unavailable"
                          : "Inventory import cleared after the active "
                            "character changed",
            .available = false,
        });
    } else if (inventory_character_arrived) {
        inventory_character_name_ = snapshot.name;
    }
    export_inventory_action_->setEnabled(snapshot.available());
    equipment_table_->setRowCount(0);
    if (!snapshot.available()) {
        const QString detail = QString::fromStdString(snapshot.detail);
        character_name_->setText(
            snapshot.name.empty()
                ? detail
                : QString::fromStdString(snapshot.name));
        equipment_state_->setText(
            detail.isEmpty() ? "Character information unavailable" : detail);
        equipment_state_->show();
        set_vital_unavailable(health_bar_, "HP");
        set_vital_unavailable(mana_bar_, "MP");
        if (inventory_reconcile_changed) {
            render_inventory_reconciliation(reconcile_inventory_entries(
                inventory_snapshot_.entries,
                inventory_snapshot_.source_name,
                character_snapshot_));
        }
        return;
    }
    character_name_->setText(QString::fromStdString(snapshot.name));
    if (snapshot.equipment.empty()) {
        equipment_state_->setText("No equipped items reported");
        equipment_state_->show();
    } else {
        equipment_state_->clear();
        equipment_state_->hide();
    }
    update_vital_bar(health_bar_, "HP", snapshot.health);
    update_vital_bar(mana_bar_, "MP", snapshot.mana);
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
    if (inventory_reconcile_changed) {
        render_inventory_reconciliation(reconcile_inventory_entries(
            inventory_snapshot_.entries,
            inventory_snapshot_.source_name,
            character_snapshot_));
    }
}

void MainWindow::render_inventory_reconciliation(
    const InventoryReconciliationSnapshot& snapshot) {
    inventory_snapshot_ = snapshot;
    if (!snapshot.available) {
        inventory_path_.clear();
        inventory_character_name_.clear();
    }
    inventory_reconciliation_->setRowCount(
        static_cast<int>(snapshot.entries.size()));
    for (std::size_t row = 0U; row < snapshot.entries.size(); ++row) {
        const InventoryEntrySnapshot& entry = snapshot.entries[row];
        const int table_row = static_cast<int>(row);
        inventory_reconciliation_->setItem(
            table_row, 0,
            new QTableWidgetItem(QString::fromStdString(entry.location)));
        inventory_reconciliation_->setItem(
            table_row, 1,
            new QTableWidgetItem(QString::fromStdString(entry.item)));
        inventory_reconciliation_->setItem(
            table_row, 2,
            new QTableWidgetItem(QString::number(entry.quantity)));
    }
    QString detail = QString::fromStdString(snapshot.detail);
    if (!snapshot.equipped_not_in_import.empty()) {
        detail += QString(" | %1 equipped item(s) missing")
                      .arg(static_cast<qulonglong>(
                          snapshot.equipped_not_in_import.size()));
    }
    inventory_state_->setText(detail);
}

void MainWindow::update_activity_snapshot(
    const ActivityAnalyticsSnapshot& analytics) {
    const bool payload_changed =
        !same_activity_export_payload(activity_analytics_, analytics);
    activity_analytics_ = analytics;
    const bool has_activity = !analytics.events.empty() ||
                              !analytics.abilities.empty();
    export_activity_action_->setEnabled(has_activity);
    delete_activity_action_->setEnabled(
        !analytics.storage_key.empty());
    if (!analytics.available) {
        const QString detail = QString::fromStdString(analytics.detail);
        activity_state_->setText(
            detail.isEmpty() ? "Activity unavailable" : detail);
        activity_state_->show();
        const QString xp_summary =
            experience_summary(character_snapshot_, analytics);
        activity_xp_summary_->setText(xp_summary);
        activity_aa_summary_->setText("AA: unavailable");
        activity_latest_summary_->setText(
            detail.isEmpty() ? "Latest: unavailable"
                             : QString("Latest: %1").arg(detail));
        activity_xp_summary_->setToolTip(activity_xp_summary_->text());
        activity_aa_summary_->setToolTip(activity_aa_summary_->text());
        activity_latest_summary_->setToolTip(
            activity_latest_summary_->text());
        if (payload_changed) {
            activity_events_->setRowCount(0);
            activity_abilities_->setRowCount(0);
        }
        return;
    }
    const QString aa_total = analytics.alternate_advancement_points
                                 ? QString::number(
                                       *analytics.alternate_advancement_points)
                                 : QString("unavailable");
    const QString aa_progress = analytics.alternate_advancement_percent
                                    ? QString::number(
                                          *analytics.alternate_advancement_percent,
                                          'f',
                                          3) +
                                          "%"
                                    : QString("unavailable");
    const QString aa_eta = analytics.next_alternate_advancement_hours
                               ? QString::number(
                                     *analytics.next_alternate_advancement_hours,
                                     'f', 1) +
                                     "h"
                               : QString("collecting");
    const QString xp_summary =
        experience_summary(character_snapshot_, analytics);
    const QString aa_summary =
        QString("AA: %1 | banked: %2 | %3/h | next: %4")
            .arg(aa_progress)
            .arg(aa_total)
            .arg(analytics.alternate_advancement_points_per_hour, 0, 'f', 2)
            .arg(aa_eta);
    QString latest = analytics.events.empty()
                         ? QString("No recent activity")
                         : QString::fromStdString(analytics.events.back().label);
    if (analytics.recent_celebration) {
        latest = QString::fromStdString(*analytics.recent_celebration);
    }
    QString latest_summary = QString("Latest: %1").arg(latest);
    if (!analytics.persisted) {
        latest_summary += " | Activity storage error";
        const QString detail = QString::fromStdString(analytics.detail);
        activity_state_->setText(
            detail.isEmpty() ? "Activity storage error" : detail);
        activity_state_->show();
    } else {
        activity_state_->clear();
        activity_state_->hide();
    }
    if (!analytics.class_activity_summary.empty()) {
        latest_summary += QString(" | %1").arg(QString::fromStdString(
            analytics.class_activity_summary));
    }
    activity_xp_summary_->setText(xp_summary);
    activity_xp_summary_->setToolTip(xp_summary);
    activity_aa_summary_->setText(aa_summary);
    activity_aa_summary_->setToolTip(aa_summary);
    activity_latest_summary_->setText(latest_summary);
    activity_latest_summary_->setToolTip(latest_summary);

    if (!payload_changed) {
        return;
    }

    activity_events_->setRowCount(
        static_cast<int>(analytics.events.size()));
    for (std::size_t offset = 0U; offset < analytics.events.size(); ++offset) {
        const std::size_t index = analytics.events.size() - offset - 1U;
        const ActivityEventSnapshot& event = analytics.events[index];
        const int row = static_cast<int>(offset);
        activity_events_->setItem(
            row, 0, new QTableWidgetItem(activity_kind_label(event.kind)));
        activity_events_->setItem(
            row, 1,
            new QTableWidgetItem(QString::fromStdString(event.zone)));
        QString label = QString::fromStdString(event.label);
        if (event.kind == ActivityEventKind::experience) {
            label += QString(" (%1%)").arg(event.amount, 0, 'f', 3);
        } else if (event.kind ==
                       ActivityEventKind::alternate_advancement &&
                   event.total) {
            label += QString(" (%1 %2; total %3)")
                         .arg(event.amount, 0, 'f', 0)
                         .arg(event.amount == 1.0 ? "point" : "points")
                         .arg(*event.total);
        } else if (event.total) {
            label += QString(" (total %1)").arg(*event.total);
        }
        activity_events_->setItem(row, 2, new QTableWidgetItem(label));
        activity_events_->setItem(
            row, 3,
            new QTableWidgetItem(QString::fromStdString(event.evidence)));
    }

    activity_abilities_->setRowCount(
        static_cast<int>(analytics.abilities.size()));
    for (std::size_t row = 0U; row < analytics.abilities.size(); ++row) {
        const AbilityActivitySnapshot& ability = analytics.abilities[row];
        const int table_row = static_cast<int>(row);
        activity_abilities_->setItem(
            table_row, 0,
            new QTableWidgetItem(QString::fromStdString(ability.name)));
        activity_abilities_->setItem(
            table_row, 1,
            new QTableWidgetItem(QString::fromStdString(ability.category)));
        activity_abilities_->setItem(
            table_row, 2,
            new QTableWidgetItem(QString::number(ability.damage)));
        activity_abilities_->setItem(
            table_row, 3,
            new QTableWidgetItem(QString::number(ability.observations)));
        activity_abilities_->setItem(
            table_row, 4,
            new QTableWidgetItem(QString::fromStdString(ability.confidence)));
    }
}

void MainWindow::render_combat_encounter(
    const CombatEncounterSnapshot& snapshot) {
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
        parse_table_->item(table_row, 0)->setToolTip(
            QString("Melee %1 | Spell %2 | DoT %3 | Pet %4")
                .arg(participant.melee_damage)
                .arg(participant.spell_damage)
                .arg(participant.damage_over_time)
                .arg(participant.pet_damage));
    }
    std::size_t ability_rows = 0U;
    for (const auto& participant : snapshot.participants) {
        ability_rows += std::min(
            participant.abilities.size(),
            kMaximumCombatAbilityRows - ability_rows);
        if (ability_rows == kMaximumCombatAbilityRows) {
            break;
        }
    }
    combat_abilities_->setRowCount(static_cast<int>(ability_rows));
    int ability_row = 0;
    for (const auto& participant : snapshot.participants) {
        for (const auto& ability : participant.abilities) {
            if (static_cast<std::size_t>(ability_row) == ability_rows) {
                break;
            }
            combat_abilities_->setItem(ability_row, 0, new QTableWidgetItem(
                QString::fromStdString(participant.name)));
            combat_abilities_->setItem(ability_row, 1, new QTableWidgetItem(
                QString::fromStdString(ability.name)));
            combat_abilities_->setItem(ability_row, 2, new QTableWidgetItem(
                QString::fromStdString(ability.category)));
            combat_abilities_->setItem(ability_row, 3, new QTableWidgetItem(
                QString::number(ability.damage)));
            combat_abilities_->setItem(ability_row, 4, new QTableWidgetItem(
                QString::number(ability.hits)));
            ++ability_row;
        }
        if (static_cast<std::size_t>(ability_row) == ability_rows) {
            break;
        }
    }
    combat_healing_->setRowCount(static_cast<int>(snapshot.healers.size()));
    for (std::size_t row = 0U; row < snapshot.healers.size(); ++row) {
        const CombatHealerSnapshot& healer = snapshot.healers[row];
        const int table_row = static_cast<int>(row);
        combat_healing_->setItem(table_row, 0, new QTableWidgetItem(
            QString::fromStdString(healer.name)));
        combat_healing_->setItem(table_row, 1, new QTableWidgetItem(
            QString::number(healer.healing)));
        combat_healing_->setItem(table_row, 2, new QTableWidgetItem(
            QString::number(healer.casts)));
        combat_healing_->setItem(table_row, 3, new QTableWidgetItem(
            QString::number(healer.hps, 'f', 1)));
        combat_healing_->setItem(table_row, 4, new QTableWidgetItem(
            QString::number(healer.percentage, 'f', 1)));
    }
    combat_timeline_->setRowCount(static_cast<int>(snapshot.timeline.size()));
    for (std::size_t row = 0U; row < snapshot.timeline.size(); ++row) {
        const CombatTimelinePoint& point = snapshot.timeline[row];
        const int table_row = static_cast<int>(row);
        combat_timeline_->setItem(table_row, 0, new QTableWidgetItem(
            QString::number(point.elapsed_seconds)));
        combat_timeline_->setItem(table_row, 1, new QTableWidgetItem(
            QString::number(point.damage)));
        combat_timeline_->setItem(table_row, 2, new QTableWidgetItem(
            QString::number(point.healing)));
    }
    if (!snapshot.available()) {
        parse_state_->setText(QString::fromStdString(snapshot.detail));
        return;
    }
    parse_state_->setText(
        QString("%1 - %2 - %3 damage - %4 healing - %5 s")
            .arg(QString::fromStdString(snapshot.detail))
            .arg(QString::fromStdString(snapshot.target))
            .arg(snapshot.total_damage)
            .arg(snapshot.total_healing)
            .arg(snapshot.duration_seconds, 0, 'f', 1));
}

void MainWindow::update_combat_snapshot(
    const CombatAnalyticsSnapshot& analytics) {
    combat_analytics_ = analytics;
    const double current_dps =
        analytics.encounter.state == CombatEncounterState::active
            ? analytics.encounter.active_character_dps
            : 0.0;
    current_dps_->setText(
        QString("DPS: %1").arg(current_dps, 0, 'f', 1));
    updating_combat_history_ = true;
    combat_history_->setRowCount(
        static_cast<int>(combat_analytics_.history.size()));
    int selected_row = -1;
    for (std::size_t offset = 0U;
         offset < combat_analytics_.history.size(); ++offset) {
        const std::size_t index =
            combat_analytics_.history.size() - offset - 1U;
        const CombatEncounterSnapshot& encounter =
            combat_analytics_.history[index];
        const int row = static_cast<int>(offset);
        combat_history_->setItem(row, 0, new QTableWidgetItem(
            QString::fromStdString(encounter.zone)));
        combat_history_->setItem(row, 1, new QTableWidgetItem(
            QString::fromStdString(encounter.target)));
        combat_history_->setItem(row, 2, new QTableWidgetItem(
            QString::number(encounter.total_damage)));
        combat_history_->setItem(row, 3, new QTableWidgetItem(
            QString::number(encounter.total_healing)));
        combat_history_->setItem(row, 4, new QTableWidgetItem(
            QString::number(encounter.duration_seconds, 'f', 1)));
        if (selected_combat_history_ &&
            *selected_combat_history_ == encounter.started_unix_seconds) {
            selected_row = row;
        }
    }
    if (selected_row >= 0) {
        combat_history_->selectRow(selected_row);
    } else {
        selected_combat_history_.reset();
        combat_history_->clearSelection();
    }
    updating_combat_history_ = false;
    combat_overview_->setText(
        QString("Zone: %1 completed | %2 damage | %3 healing%4%5")
            .arg(analytics.zone_encounters)
            .arg(analytics.zone_damage)
            .arg(analytics.zone_healing)
            .arg(analytics.history_retention_enabled
                     ? QString{}
                     : QString(" | Retention off"))
            .arg(analytics.history_persisted
                     ? QString{}
                     : QString(" | %1").arg(
                           QString::fromStdString(analytics.history_detail))));
    if (selected_row >= 0) {
        const std::size_t index =
            combat_analytics_.history.size() -
            static_cast<std::size_t>(selected_row) - 1U;
        render_combat_encounter(combat_analytics_.history[index]);
    } else {
        render_combat_encounter(combat_analytics_.encounter);
    }
    if (!combat_analytics_.encounter.available()) {
        parse_state_->setText(QString::fromStdString(
            combat_analytics_.encounter.detail));
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
    std::optional<std::array<int, 4>> activity_summary_widths;
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
        retain_combat_history_action_->setChecked(
            state->combat_history_enabled);
        retain_activity_history_action_->setChecked(
            state->activity_history_enabled);
        activity_summary_widths = state->activity_summary_widths;
        activity_summary_widths_ = state->activity_summary_widths;
    }
    QTimer::singleShot(
        0, this, [this, activity_summary_widths]() {
            ensure_on_screen();
            if (activity_summary_widths) {
                activity_summary_splitter_->setSizes(
                    {(*activity_summary_widths)[0],
                     (*activity_summary_widths)[1],
                     (*activity_summary_widths)[2],
                     (*activity_summary_widths)[3]});
                activity_summary_widths_ = *activity_summary_widths;
            }
        });
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

bool MainWindow::save_ui_state() {
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
    if (activity_summary_splitter_->isVisible()) {
        const QList<int> summary_sizes = activity_summary_splitter_->sizes();
        if (summary_sizes.size() == 4) {
            activity_summary_widths_ =
                {summary_sizes[0], summary_sizes[1],
                 summary_sizes[2], summary_sizes[3]};
        }
    }
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
        .combat_history_enabled = combat_history_enabled(),
        .activity_history_enabled = activity_history_enabled(),
        .activity_summary_widths = activity_summary_widths_,
        .spawn_filter = spawn_filter_->text(),
        .spawn_type_filter = type_filter,
        .spawn_sort_column = header->sortIndicatorSection(),
        .spawn_sort_descending =
            header->sortIndicatorOrder() == Qt::DescendingOrder,
        .spawn_column_widths = column_widths,
    };
    return settings_.save(state);
}

void MainWindow::save_combat_history_preference() {
    auto state = settings_.load();
    if (!state) {
        (void)save_ui_state();
        return;
    }
    state->combat_history_enabled = combat_history_enabled();
    (void)settings_.save(*state);
}

void MainWindow::save_activity_history_preference() {
    const bool requested = activity_history_enabled();
    auto state = settings_.load();
    bool saved = false;
    bool previous = !requested;
    if (!state) {
        saved = save_ui_state();
    } else {
        previous = state->activity_history_enabled;
        state->activity_history_enabled = requested;
        saved = settings_.save(*state);
    }
    if (saved) {
        return;
    }
    const QSignalBlocker blocker(retain_activity_history_action_);
    retain_activity_history_action_->setChecked(previous);
    statusBar()->showMessage(
        "Activity retention preference could not be saved; the previous "
        "setting was restored");
}

bool MainWindow::combat_history_enabled() const {
    return retain_combat_history_action_ != nullptr &&
           retain_combat_history_action_->isChecked();
}

bool MainWindow::activity_history_enabled() const {
    return retain_activity_history_action_ != nullptr &&
           retain_activity_history_action_->isChecked();
}

void MainWindow::changeEvent(QEvent* event) {
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::WindowStateChange) {
        update_maximize_button();
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    (void)save_ui_state();
    QMainWindow::closeEvent(event);
    QCoreApplication::quit();
}

}  // namespace plazmic
