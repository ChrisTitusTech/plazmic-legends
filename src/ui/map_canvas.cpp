#include "ui/map_canvas.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QResizeEvent>
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

}  // namespace

MapCanvas::MapCanvas(QWidget* parent) : QWidget(parent) {
    setObjectName("map-canvas");
    setMinimumSize(360, 260);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
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
}

void MapCanvas::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
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
