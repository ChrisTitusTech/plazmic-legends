#include "ui/main_window.h"

#include <algorithm>
#include <ranges>
#include <utility>

#include <QCloseEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFrame>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QScreen>
#include <QStatusBar>
#include <QTableWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace plazmic {
namespace {

QLabel* placeholder_label(const QString& title, const QString& detail) {
    auto* label = new QLabel(
        QString("<h2>%1</h2><p>%2</p>").arg(title, detail));
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);
    label->setObjectName(title.toLower().replace(' ', '-') + "-placeholder");
    return label;
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

    auto* map_frame = new QFrame;
    map_frame->setObjectName("map-view");
    map_frame->setFrameShape(QFrame::StyledPanel);
    auto* map_layout = new QVBoxLayout(map_frame);
    map_layout->addWidget(placeholder_label(
        "Map",
        "Map loading and the validated player marker begin in Phase 3."));
    setCentralWidget(map_frame);

    auto* spawn_container = new QWidget;
    auto* spawn_layout = new QVBoxLayout(spawn_container);
    auto* spawn_notice = new QLabel(
        "Spawn acquisition remains unavailable until Phase 4.");
    spawn_notice->setObjectName("spawn-unavailable");
    spawn_notice->setWordWrap(true);
    spawn_layout->addWidget(spawn_notice);
    auto* spawn_table = new QTableWidget(0, 4);
    spawn_table->setObjectName("spawn-table");
    spawn_table->setHorizontalHeaderLabels(
        {"Name", "Level", "Type", "Distance"});
    spawn_table->horizontalHeader()->setStretchLastSection(true);
    spawn_table->setSortingEnabled(true);
    spawn_layout->addWidget(spawn_table);
    auto* spawn_dock = create_dock(
        "Spawns", "spawn-dock", spawn_container, this);
    addDockWidget(Qt::RightDockWidgetArea, spawn_dock);

    auto* detail_dock = create_dock(
        "Details", "detail-dock",
        placeholder_label(
            "Selection",
            "No selection. Map and spawn selection arrive in Phase 4."),
        this);
    addDockWidget(Qt::BottomDockWidgetArea, detail_dock);

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

void MainWindow::restore_ui_state() {
    if (!reset_layout_) {
        if (const auto state = settings_.load()) {
            const bool layout_restored = restoreState(state->layout);
            const bool geometry_restored = restoreGeometry(state->geometry);
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
    const UiState state{
        .geometry = saveGeometry(),
        .layout = saveState(),
    };
    (void)settings_.save(state);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    save_ui_state();
    QMainWindow::closeEvent(event);
    QCoreApplication::quit();
}

}  // namespace plazmic
