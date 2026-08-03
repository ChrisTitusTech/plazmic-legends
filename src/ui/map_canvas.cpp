#include "ui/map_canvas.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <ranges>

#include <QAction>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QFontMetricsF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPolygonF>
#include <QResizeEvent>
#include <QToolButton>
#include <QWheelEvent>

namespace plazmic {
namespace {

QPointF screen_point(const MapViewport& viewport,
                     const MapPosition& point) {
    const MapPoint2D screen =
        viewport.map_to_screen({point.x, point.y});
    return {screen.x, screen.y};
}

QColor readable_map_color(const MapColor& color,
                          const QPalette& palette) {
    QColor result(color.red, color.green, color.blue);
    const QColor background = palette.color(QPalette::Base);
    if (std::abs(result.lightness() - background.lightness()) < 42) {
        result = palette.color(QPalette::Text);
    }
    return result;
}

QAction* add_toggle_action(QMenu* menu,
                           const QString& text,
                           bool checked) {
    QAction* action = menu->addAction(text);
    action->setCheckable(true);
    action->setChecked(checked);
    return action;
}

}  // namespace

QColor spawn_marker_color(SpawnPresentationCategory category,
                          unsigned int player_level,
                          unsigned int spawn_level) {
    switch (category) {
        case SpawnPresentationCategory::player:
            return {65, 160, 255};
        case SpawnPresentationCategory::named_npc:
            return {240, 165, 35};
        case SpawnPresentationCategory::npc: {
            switch (spawn_consider_color(player_level, spawn_level)) {
                case SpawnConsiderColor::gray:
                    return {145, 145, 145};
                case SpawnConsiderColor::green:
                    return {65, 190, 90};
                case SpawnConsiderColor::light_blue:
                    return {95, 200, 255};
                case SpawnConsiderColor::blue:
                    return {45, 105, 255};
                case SpawnConsiderColor::white:
                    return {245, 245, 245};
                case SpawnConsiderColor::yellow:
                    return {255, 220, 45};
                case SpawnConsiderColor::red:
                    return {235, 70, 60};
            }
            return {145, 145, 145};
        }
        case SpawnPresentationCategory::other:
            return {145, 145, 145};
    }
    return {145, 145, 145};
}

MapCanvas::MapCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("map-canvas");
    setMinimumSize(360, 260);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    filter_button_ = new QToolButton(this);
    filter_button_->setObjectName("spawn-filter-menu-button");
    filter_button_->setText("Filters / Labels");
    filter_button_->setToolTip(
        "Show or hide spawn categories and map labels");
    filter_button_->setPopupMode(QToolButton::InstantPopup);
    filter_menu_ = new QMenu("Filters / Labels", filter_button_);

    QMenu* markers_menu = filter_menu_->addMenu("Show markers");
    named_spawns_action_ = add_toggle_action(
        markers_menu, "Named NPCs", named_spawns_visible_);
    player_spawns_action_ = add_toggle_action(
        markers_menu, "PCs", player_spawns_visible_);
    npc_spawns_action_ = add_toggle_action(
        markers_menu, "NPCs", npc_spawns_visible_);
    other_spawns_action_ = add_toggle_action(
        markers_menu, "Ground / Other", other_spawns_visible_);
    connect(named_spawns_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_named_spawns_visible(visible);
            });
    connect(player_spawns_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_player_spawns_visible(visible);
            });
    connect(npc_spawns_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_npc_spawns_visible(visible);
            });
    connect(other_spawns_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_other_spawns_visible(visible);
            });

    QMenu* labels_menu = filter_menu_->addMenu("Show labels");
    named_labels_action_ = add_toggle_action(
        labels_menu, "Named NPCs", named_spawn_labels_visible_);
    player_labels_action_ = add_toggle_action(
        labels_menu, "PCs", player_labels_visible_);
    npc_labels_action_ = add_toggle_action(
        labels_menu, "NPCs", npc_labels_visible_);
    connect(named_labels_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_named_spawn_labels_visible(visible);
            });
    connect(player_labels_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_player_labels_visible(visible);
            });
    connect(npc_labels_action_, &QAction::toggled, this,
            [this](bool visible) {
                set_npc_labels_visible(visible);
            });
    filter_button_->setMenu(filter_menu_);
    filter_button_->adjustSize();
}

void MapCanvas::set_zone_map(ZoneMap map) {
    if (map.record_count() > kMaximumRenderableMapRecords) {
        clear_zone_map(
            "Map exceeds the bounded renderer record limit");
        return;
    }
    map_ = std::move(map);
    bounds_ = calculate_map_bounds(*map_);
    unsigned int maximum_layer = 0;
    for (const MapLayer& layer : map_->layers) {
        maximum_layer = std::max(maximum_layer, layer.index);
    }
    visible_layers_.assign(
        static_cast<std::size_t>(maximum_layer) + 1U, true);
    empty_detail_.clear();
    needs_fit_ = true;
    map_cache_dirty_ = true;
    refresh_height_filter_center(true);
    update();
}

void MapCanvas::clear_zone_map(QString detail) {
    map_.reset();
    bounds_.reset();
    visible_layers_.clear();
    height_filter_center_.reset();
    empty_detail_ = std::move(detail);
    needs_fit_ = true;
    map_cache_dirty_ = true;
    map_cache_ = {};
    update();
}

void MapCanvas::set_player_snapshot(PlayerSnapshot snapshot) {
    const bool position_changed =
        snapshot.zone != player_.zone || snapshot.x != player_.x ||
        snapshot.y != player_.y;
    player_ = std::move(snapshot);
    refresh_height_filter_center(false);
    if (player_follow_enabled_ && position_changed && map_ &&
        player_.available() && player_.zone == map_->zone) {
        if (needs_fit_) {
            fit_map();
        }
        viewport_.center_on(player_map_position(player_));
        map_cache_dirty_ = true;
    }
    update();
}

void MapCanvas::set_spawn_snapshot(SpawnCollectionSnapshot snapshot) {
    if (spawns_ == snapshot) {
        return;
    }
    spawns_ = std::move(snapshot);
    if (!spawns_.available()) {
        selected_spawn_.reset();
    } else if (selected_spawn_ &&
               std::ranges::none_of(
                   spawns_.spawns,
                   [this](const SpawnSnapshot& spawn) {
                       return spawn.id == *selected_spawn_;
                   })) {
        selected_spawn_.reset();
    }
    update();
}

void MapCanvas::set_selected_spawn(
    std::optional<std::uint32_t> id) {
    if (selected_spawn_ == id) {
        return;
    }
    selected_spawn_ = id;
    update();
}

void MapCanvas::set_spawn_selected_callback(
    std::function<void(std::uint32_t)> callback) {
    spawn_selected_callback_ = std::move(callback);
}

void MapCanvas::set_layer_visible(unsigned int layer, bool visible) {
    const std::size_t index = static_cast<std::size_t>(layer);
    if (index >= visible_layers_.size()) {
        return;
    }
    visible_layers_[index] = visible;
    map_cache_dirty_ = true;
    update();
}

void MapCanvas::set_height_filter_enabled(bool enabled) {
    if (height_filter_enabled_ == enabled) {
        return;
    }
    height_filter_enabled_ = enabled;
    refresh_height_filter_center(true);
    map_cache_dirty_ = true;
    update();
}

void MapCanvas::set_height_filter_range(double below, double above) {
    if (!std::isfinite(below) || !std::isfinite(above)) {
        return;
    }
    const double bounded_below =
        std::clamp(below, 0.0, kMaximumHeightFilterRange);
    const double bounded_above =
        std::clamp(above, 0.0, kMaximumHeightFilterRange);
    if (height_filter_below_ == bounded_below &&
        height_filter_above_ == bounded_above) {
        return;
    }
    height_filter_below_ = bounded_below;
    height_filter_above_ = bounded_above;
    map_cache_dirty_ = true;
    update();
}

void MapCanvas::set_player_follow_enabled(bool enabled) {
    if (player_follow_enabled_ == enabled) {
        return;
    }
    player_follow_enabled_ = enabled;
    if (enabled && map_ && player_.available() &&
        player_.zone == map_->zone) {
        if (needs_fit_) {
            fit_map();
        }
        viewport_.center_on(player_map_position(player_));
        map_cache_dirty_ = true;
    }
    update();
}

void MapCanvas::set_named_spawn_labels_visible(bool visible) {
    if (named_spawn_labels_visible_ == visible) {
        return;
    }
    named_spawn_labels_visible_ = visible;
    named_labels_action_->setChecked(visible);
    update();
}

void MapCanvas::set_player_labels_visible(bool visible) {
    if (player_labels_visible_ == visible) {
        return;
    }
    player_labels_visible_ = visible;
    player_labels_action_->setChecked(visible);
    update();
}

void MapCanvas::set_npc_labels_visible(bool visible) {
    if (npc_labels_visible_ == visible) {
        return;
    }
    npc_labels_visible_ = visible;
    npc_labels_action_->setChecked(visible);
    update();
}

void MapCanvas::set_named_spawns_visible(bool visible) {
    if (named_spawns_visible_ == visible) {
        return;
    }
    named_spawns_visible_ = visible;
    named_spawns_action_->setChecked(visible);
    update();
}

void MapCanvas::set_player_spawns_visible(bool visible) {
    if (player_spawns_visible_ == visible) {
        return;
    }
    player_spawns_visible_ = visible;
    player_spawns_action_->setChecked(visible);
    update();
}

void MapCanvas::set_npc_spawns_visible(bool visible) {
    if (npc_spawns_visible_ == visible) {
        return;
    }
    npc_spawns_visible_ = visible;
    npc_spawns_action_->setChecked(visible);
    update();
}

void MapCanvas::set_other_spawns_visible(bool visible) {
    if (other_spawns_visible_ == visible) {
        return;
    }
    other_spawns_visible_ = visible;
    other_spawns_action_->setChecked(visible);
    update();
}

void MapCanvas::reset_view() {
    player_follow_enabled_ = false;
    needs_fit_ = true;
    fit_map();
    update();
}

bool MapCanvas::layer_visible(unsigned int layer) const {
    const std::size_t index = static_cast<std::size_t>(layer);
    return index < visible_layers_.size() && visible_layers_[index];
}

bool MapCanvas::spawn_visible(const SpawnSnapshot& spawn) const {
    if (!map_ || !spawns_.available() ||
        spawns_.zone != map_->zone ||
        !spawn_category_visible(spawn_presentation_category(spawn))) {
        return false;
    }
    return !height_filter_center_ ||
           map_height_range_visible(
               spawn.z, spawn.z, *height_filter_center_,
               height_filter_below_, height_filter_above_);
}

bool MapCanvas::spawn_category_visible(
    SpawnPresentationCategory category) const {
    switch (category) {
        case SpawnPresentationCategory::player:
            return player_spawns_visible_;
        case SpawnPresentationCategory::named_npc:
            return named_spawns_visible_;
        case SpawnPresentationCategory::npc:
            return npc_spawns_visible_;
        case SpawnPresentationCategory::other:
            return other_spawns_visible_;
    }
    return false;
}

bool MapCanvas::spawn_label_visible(const SpawnSnapshot& spawn) const {
    switch (spawn_presentation_category(spawn)) {
        case SpawnPresentationCategory::player:
            return player_labels_visible_;
        case SpawnPresentationCategory::named_npc:
            return named_spawn_labels_visible_;
        case SpawnPresentationCategory::npc:
            return npc_labels_visible_;
        case SpawnPresentationCategory::other:
            return false;
    }
    return false;
}

std::optional<std::uint32_t> MapCanvas::spawn_at_screen_point(
    const QPointF& point) const {
    std::optional<std::uint32_t> closest;
    double closest_distance = 11.0;
    for (const SpawnSnapshot& spawn : spawns_.spawns) {
        if (!spawn_visible(spawn)) {
            continue;
        }
        const MapPoint2D screen =
            viewport_.map_to_screen(spawn_map_position(spawn));
        const double distance =
            std::hypot(point.x() - screen.x, point.y() - screen.y);
        if (distance <= closest_distance) {
            closest_distance = distance;
            closest = spawn.id;
        }
    }
    return closest;
}

void MapCanvas::refresh_height_filter_center(bool force) {
    std::optional<double> next_center;
    if (height_filter_enabled_ && map_ && player_.available() &&
        player_.zone == map_->zone) {
        next_center = player_.z;
    }

    const bool changed =
        force ||
        next_center.has_value() != height_filter_center_.has_value() ||
        (next_center && height_filter_center_ &&
         *next_center != *height_filter_center_);
    if (changed) {
        height_filter_center_ = next_center;
        map_cache_dirty_ = true;
    }
}

void MapCanvas::adjust_height_filter_range() {
    QDialog dialog(this);
    dialog.setWindowTitle("Map height filter");

    auto* layout = new QFormLayout(&dialog);
    auto* below = new QDoubleSpinBox(&dialog);
    auto* above = new QDoubleSpinBox(&dialog);
    for (QDoubleSpinBox* input : {below, above}) {
        input->setRange(0.0, kMaximumHeightFilterRange);
        input->setDecimals(1);
        input->setSingleStep(5.0);
        input->setSuffix(" units");
    }
    below->setValue(height_filter_below_);
    above->setValue(height_filter_above_);
    layout->addRow("Below player Z:", below);
    layout->addRow("Above player Z:", above);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog,
            &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog,
            &QDialog::reject);
    layout->addRow(buttons);

    if (dialog.exec() == QDialog::Accepted) {
        set_height_filter_range(below->value(), above->value());
    }
}

void MapCanvas::fit_map() {
    if (!bounds_ || width() <= 0 || height() <= 0) {
        return;
    }
    viewport_.fit(*bounds_, static_cast<double>(width()),
                  static_cast<double>(height()));
    needs_fit_ = false;
    map_cache_dirty_ = true;
}

void MapCanvas::render_map_cache() {
    map_cache_ = QPixmap(size());
    map_cache_.fill(palette().color(QPalette::Base));
    QPainter painter(&map_cache_);
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (const MapLayer& layer : map_->layers) {
        if (!layer_visible(layer.index)) {
            continue;
        }
        for (const MapLineRecord& line : layer.lines) {
            if (height_filter_center_ &&
                !map_height_range_visible(
                    line.start.z, line.end.z,
                    *height_filter_center_,
                    height_filter_below_,
                    height_filter_above_)) {
                continue;
            }
            QPen pen(readable_map_color(line.color, palette()));
            pen.setWidthF(layer.index == 0U ? 1.1 : 0.8);
            painter.setPen(pen);
            painter.drawLine(
                screen_point(viewport_, line.start),
                screen_point(viewport_, line.end));
        }
    }

    painter.setRenderHint(QPainter::Antialiasing, true);
    for (const MapLayer& layer : map_->layers) {
        if (!layer_visible(layer.index)) {
            continue;
        }
        for (const MapLabelRecord& label : layer.labels) {
            if (height_filter_center_ &&
                !map_height_range_visible(
                    label.position.z, label.position.z,
                    *height_filter_center_,
                    height_filter_below_,
                    height_filter_above_)) {
                continue;
            }
            painter.setPen(readable_map_color(label.color, palette()));
            QFont font = painter.font();
            font.setPointSize(std::clamp(label.size + 6, 7, 14));
            painter.setFont(font);
            const QPointF position =
                screen_point(viewport_, label.position);
            painter.drawText(
                position + QPointF(3.0, -3.0),
                QString::fromStdString(label.text));
        }
    }
    map_cache_dirty_ = false;
}

void MapCanvas::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if (!map_ || !bounds_) {
        painter.fillRect(rect(), palette().color(QPalette::Base));
        painter.setPen(palette().color(QPalette::Text));
        painter.drawText(
            rect().adjusted(24, 24, -24, -24),
            Qt::AlignCenter | Qt::TextWordWrap,
            empty_detail_);
        return;
    }
    if (needs_fit_) {
        fit_map();
    }
    if (map_cache_dirty_ || map_cache_.size() != size()) {
        render_map_cache();
    }
    painter.drawPixmap(0, 0, map_cache_);

    if (spawns_.available() && spawns_.zone == map_->zone) {
        for (const SpawnSnapshot& spawn : spawns_.spawns) {
            if (!spawn_visible(spawn)) {
                continue;
            }
            const MapPoint2D screen =
                viewport_.map_to_screen(spawn_map_position(spawn));
            const QPointF center(screen.x, screen.y);
            const SpawnPresentationCategory category =
                spawn_presentation_category(spawn);
            const QColor color = spawn_marker_color(
                category, spawns_.player_level, spawn.level);
            const bool selected =
                selected_spawn_ && *selected_spawn_ == spawn.id;
            QPen pen(selected
                         ? QColor(255, 210, 55)
                         : (category == SpawnPresentationCategory::npc
                                ? palette().color(QPalette::Text)
                                : color));
            pen.setWidthF(selected ? 2.5 : 1.0);
            painter.setPen(pen);
            painter.setBrush(color);
            const double radius = selected ? 6.0 : 3.5;
            if (category == SpawnPresentationCategory::named_npc) {
                painter.drawPolygon(QPolygonF{
                    center + QPointF(0.0, -radius - 1.5),
                    center + QPointF(radius + 1.5, 0.0),
                    center + QPointF(0.0, radius + 1.5),
                    center + QPointF(-radius - 1.5, 0.0),
                });
            } else if (category == SpawnPresentationCategory::other) {
                painter.drawRect(QRectF(
                    center.x() - radius, center.y() - radius,
                    radius * 2.0, radius * 2.0));
            } else {
                painter.drawEllipse(center, radius, radius);
            }

            if (spawn_label_visible(spawn)) {
                painter.save();
                const QString text =
                    QString::fromStdString(spawn.name);
                QFont label_font = painter.font();
                label_font.setBold(
                    category == SpawnPresentationCategory::named_npc);
                painter.setFont(label_font);
                const QFontMetricsF metrics(label_font);
                QRectF label_rect(
                    center.x() + radius + 5.0,
                    center.y() - metrics.height() / 2.0 - 2.0,
                    metrics.horizontalAdvance(text) + 8.0,
                    metrics.height() + 4.0);
                QColor background = palette().color(QPalette::Base);
                background.setAlpha(220);
                painter.setPen(Qt::NoPen);
                painter.setBrush(background);
                painter.drawRoundedRect(label_rect, 2.0, 2.0);
                painter.setPen(palette().color(QPalette::Text));
                painter.setBrush(Qt::NoBrush);
                painter.drawText(
                    label_rect.adjusted(4.0, 2.0, -4.0, -2.0),
                    Qt::AlignLeft | Qt::AlignVCenter, text);
                painter.restore();
            }
        }
    }

    if (player_.available() && player_.zone == map_->zone) {
        const MapPoint2D map_position = player_map_position(player_);
        const MapPoint2D screen = viewport_.map_to_screen(map_position);
        const QPointF center(screen.x, screen.y);
        constexpr double kMarkerRadius = 7.0;
        constexpr double kHeadingLength = 22.0;
        const double radians =
            player_map_heading_degrees(player_) *
            (std::numbers::pi / 180.0);
        const QPointF tip(
            center.x() + std::sin(radians) * kHeadingLength,
            center.y() - std::cos(radians) * kHeadingLength);

        QPen marker_pen(palette().color(QPalette::Highlight));
        marker_pen.setWidthF(3.0);
        painter.setPen(marker_pen);
        painter.setBrush(palette().color(QPalette::HighlightedText));
        painter.drawEllipse(center, kMarkerRadius, kMarkerRadius);
        painter.drawLine(center, tip);
    }

    painter.setPen(palette().color(QPalette::Text));
    const QString zone_text =
        QString("Zone: %1  Layers: %2  Records: %3  Height: %4")
            .arg(QString::fromStdString(map_->zone))
            .arg(map_->layers.size())
            .arg(map_->record_count())
            .arg(height_filter_enabled_
                     ? QString("-%1/+%2 from player Z")
                           .arg(height_filter_below_, 0, 'f', 1)
                           .arg(height_filter_above_, 0, 'f', 1)
                     : QString("All"));
    painter.drawText(12, 22, zone_text);
    if (player_.available() && player_.zone == map_->zone) {
        painter.drawText(
            12, 42,
            QString("Player: X %1  Y %2  Z %3  Heading %4")
                .arg(player_.x, 0, 'f', 1)
                .arg(player_.y, 0, 'f', 1)
                .arg(player_.z, 0, 'f', 1)
                .arg(player_.heading_degrees, 0, 'f', 1));
    }
}

void MapCanvas::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::StyleChange) {
        map_cache_dirty_ = true;
    }
    QWidget::changeEvent(event);
}

void MapCanvas::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    viewport_.resize(
        static_cast<double>(event->size().width()),
        static_cast<double>(event->size().height()));
    map_cache_dirty_ = true;
    if (needs_fit_) {
        fit_map();
    }
    filter_button_->adjustSize();
    filter_button_->move(
        std::max(8, width() - filter_button_->width() - 12), 10);
    filter_button_->raise();
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        if (const auto spawn =
                spawn_at_screen_point(event->position())) {
            set_selected_spawn(spawn);
            if (spawn_selected_callback_) {
                spawn_selected_callback_(*spawn);
            }
            event->accept();
            return;
        }
        dragging_ = true;
        drag_origin_ = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void MapCanvas::mouseMoveEvent(QMouseEvent* event) {
    if (!dragging_) {
        QWidget::mouseMoveEvent(event);
        return;
    }
    const QPoint current = event->position().toPoint();
    const QPoint delta = current - drag_origin_;
    drag_origin_ = current;
    player_follow_enabled_ = false;
    viewport_.pan(
        static_cast<double>(delta.x()),
        static_cast<double>(delta.y()));
    needs_fit_ = false;
    map_cache_dirty_ = true;
    update();
    event->accept();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        unsetCursor();
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void MapCanvas::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        reset_view();
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void MapCanvas::wheelEvent(QWheelEvent* event) {
    const double steps =
        static_cast<double>(event->angleDelta().y()) / 120.0;
    const double factor = std::pow(1.18, steps);
    viewport_.zoom_at(
        factor, event->position().x(), event->position().y());
    needs_fit_ = false;
    map_cache_dirty_ = true;
    update();
    event->accept();
}

void MapCanvas::contextMenuEvent(QContextMenuEvent* event) {
    if (!map_) {
        return;
    }
    QMenu menu(this);
    QAction* fit_action = menu.addAction("Fit map");
    connect(fit_action, &QAction::triggered, this,
            [this]() { reset_view(); });
    QAction* follow_action = menu.addAction("Follow player");
    follow_action->setCheckable(true);
    follow_action->setChecked(player_follow_enabled_);
    connect(follow_action, &QAction::toggled, this,
            [this](bool enabled) {
                set_player_follow_enabled(enabled);
            });
    QAction* height_action = menu.addAction("Height filter");
    height_action->setCheckable(true);
    height_action->setChecked(height_filter_enabled_);
    connect(height_action, &QAction::toggled, this,
            [this](bool enabled) {
                set_height_filter_enabled(enabled);
            });
    QAction* adjust_height_action =
        menu.addAction("Adjust height range...");
    connect(adjust_height_action, &QAction::triggered, this,
            [this]() { adjust_height_filter_range(); });
    menu.addSeparator();
    menu.addMenu(filter_menu_);
    menu.addSeparator();
    for (const MapLayer& layer : map_->layers) {
        QAction* action = menu.addAction(
            QString("Layer %1").arg(layer.index));
        action->setCheckable(true);
        action->setChecked(layer_visible(layer.index));
        connect(action, &QAction::toggled, this,
                [this, index = layer.index](bool visible) {
                    set_layer_visible(index, visible);
                });
    }
    menu.exec(event->globalPos());
}

}  // namespace plazmic
