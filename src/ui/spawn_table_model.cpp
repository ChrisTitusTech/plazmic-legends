#include "ui/spawn_table_model.h"

#include <algorithm>
#include <utility>

#include <QVariant>

namespace plazmic {

SpawnTableModel::SpawnTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

int SpawnTableModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid()
               ? 0
               : static_cast<int>(snapshot_.spawns.size());
}

int SpawnTableModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : column_count;
}

QVariant SpawnTableModel::data(const QModelIndex& index, int role) const {
    const SpawnSnapshot* spawn = spawn_at(index.row());
    if (spawn == nullptr || index.column() < 0 ||
        index.column() >= column_count) {
        return {};
    }
    if (role == kSpawnIdRole) {
        return QVariant::fromValue(spawn->id);
    }
    if (role == kSpawnSortRole) {
        switch (index.column()) {
            case name_column:
                return QString::fromStdString(spawn->name);
            case level_column:
                return spawn->level;
            case type_column:
                return static_cast<int>(spawn->type);
            case distance_column:
                return spawn->distance;
            case column_count:
                break;
        }
    }
    if (role != Qt::DisplayRole) {
        return {};
    }
    switch (index.column()) {
        case name_column:
            return QString::fromStdString(spawn->name);
        case level_column:
            return spawn->level;
        case type_column:
            return QString::fromLatin1(spawn_type_label(spawn->type));
        case distance_column:
            return QString::number(spawn->distance, 'f', 1);
        case column_count:
            break;
    }
    return {};
}

QVariant SpawnTableModel::headerData(int section,
                                     Qt::Orientation orientation,
                                     int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) {
        return QAbstractTableModel::headerData(
            section, orientation, role);
    }
    switch (section) {
        case name_column:
            return "Name";
        case level_column:
            return "Level";
        case type_column:
            return "Type";
        case distance_column:
            return "Distance";
        case column_count:
            break;
    }
    return {};
}

void SpawnTableModel::set_snapshot(SpawnCollectionSnapshot snapshot) {
    if (snapshot_ == snapshot) {
        return;
    }
    const bool stable_rows =
        snapshot.spawns.size() == snapshot_.spawns.size() &&
        std::equal(
            snapshot.spawns.begin(), snapshot.spawns.end(),
            snapshot_.spawns.begin(),
            [](const SpawnSnapshot& left, const SpawnSnapshot& right) {
                return left.id == right.id;
            });
    if (!stable_rows) {
        beginResetModel();
        snapshot_ = std::move(snapshot);
        endResetModel();
        return;
    }
    snapshot_ = std::move(snapshot);
    if (!snapshot_.spawns.empty()) {
        emit dataChanged(
            index(0, 0),
            index(rowCount() - 1, column_count - 1),
            {Qt::DisplayRole, kSpawnSortRole});
    }
}

const SpawnSnapshot* SpawnTableModel::spawn_at(int row) const {
    if (row < 0 ||
        static_cast<std::size_t>(row) >= snapshot_.spawns.size()) {
        return nullptr;
    }
    return &snapshot_.spawns[static_cast<std::size_t>(row)];
}

int SpawnTableModel::row_for_id(std::uint32_t id) const {
    const auto found = std::find_if(
        snapshot_.spawns.begin(), snapshot_.spawns.end(),
        [id](const SpawnSnapshot& spawn) { return spawn.id == id; });
    return found == snapshot_.spawns.end()
               ? -1
               : static_cast<int>(found - snapshot_.spawns.begin());
}

SpawnFilterProxyModel::SpawnFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent) {
    setSortRole(kSpawnSortRole);
    setDynamicSortFilter(true);
}

void SpawnFilterProxyModel::set_name_filter(QString filter) {
    if (name_filter_ == filter) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    name_filter_ = std::move(filter);
    endFilterChange(Direction::Rows);
#else
    name_filter_ = std::move(filter);
    invalidateRowsFilter();
#endif
}

void SpawnFilterProxyModel::set_type_filter(
    std::optional<SpawnType> type) {
    if (type_filter_ == type) {
        return;
    }
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    type_filter_ = type;
    endFilterChange(Direction::Rows);
#else
    type_filter_ = type;
    invalidateRowsFilter();
#endif
}

bool SpawnFilterProxyModel::filterAcceptsRow(
    int source_row,
    const QModelIndex& source_parent) const {
    const auto* model =
        static_cast<const SpawnTableModel*>(sourceModel());
    if (model == nullptr || source_parent.isValid()) {
        return false;
    }
    const SpawnSnapshot* spawn = model->spawn_at(source_row);
    if (spawn == nullptr ||
        (type_filter_ && spawn->type != *type_filter_)) {
        return false;
    }
    return name_filter_.isEmpty() ||
           QString::fromStdString(spawn->name)
               .contains(name_filter_, Qt::CaseInsensitive);
}

}  // namespace plazmic
