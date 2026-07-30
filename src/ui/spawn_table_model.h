#pragma once

#include "model/spawn_snapshot.h"

#include <cstdint>
#include <optional>

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>
#include <QString>

namespace plazmic {

inline constexpr int kSpawnIdRole = Qt::UserRole + 1;
inline constexpr int kSpawnSortRole = Qt::UserRole + 2;

class SpawnTableModel final : public QAbstractTableModel {
  public:
    enum Column {
        name_column = 0,
        level_column,
        type_column,
        distance_column,
        column_count,
    };

    explicit SpawnTableModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index,
                  int role = Qt::DisplayRole) const override;
    QVariant headerData(int section,
                        Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

    void set_snapshot(SpawnCollectionSnapshot snapshot);
    [[nodiscard]] const SpawnCollectionSnapshot& snapshot() const {
        return snapshot_;
    }
    [[nodiscard]] const SpawnSnapshot* spawn_at(int row) const;
    [[nodiscard]] int row_for_id(std::uint32_t id) const;

  private:
    SpawnCollectionSnapshot snapshot_;
};

class SpawnFilterProxyModel final : public QSortFilterProxyModel {
  public:
    explicit SpawnFilterProxyModel(QObject* parent = nullptr);

    void set_name_filter(QString filter);
    void set_type_filter(std::optional<SpawnType> type);

  protected:
    bool filterAcceptsRow(
        int source_row,
        const QModelIndex& source_parent) const override;

  private:
    QString name_filter_;
    std::optional<SpawnType> type_filter_;
};

}  // namespace plazmic
