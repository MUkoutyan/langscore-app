#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <vector>
#include <optional>

namespace langscore {

class CSVEditorTable : public QAbstractTableModel {
    Q_OBJECT
public:
    CSVEditorTable(QObject* parent = nullptr);
    explicit CSVEditorTable(const QString& path, QObject* parent = nullptr);

    // CSV file operations
    bool load(const QString& path);
    bool save(const QString& path) const;

    // QAbstractTableModel overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool insertRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    bool insertColumns(int column, int count, const QModelIndex& parent = QModelIndex()) override;
    bool removeColumns(int column, int count, const QModelIndex& parent = QModelIndex()) override;

    // Convenience methods
    size_t rowCountRaw() const;
    size_t colCountRaw() const;
    std::optional<QString> getCell(size_t row, size_t col) const;
    bool setCell(size_t row, size_t col, const QString& value);

    const std::vector<std::vector<QString>>& dataRaw() const;

private:
    std::vector<std::vector<QString>> csvData;
};

} // namespace langscore