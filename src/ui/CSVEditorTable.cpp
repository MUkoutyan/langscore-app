#include "CSVEditorTable.h"
#include "../csv.hpp"
#include <fstream>

namespace langscore {

CSVEditorTable::CSVEditorTable(QObject* parent)
    : QAbstractTableModel(parent)
{}

CSVEditorTable::CSVEditorTable(const QString& path, QObject* parent)
    : QAbstractTableModel(parent)
{
    load(path);
}

bool CSVEditorTable::load(const QString& path) {
    beginResetModel();
    csvData = readCsv(path);
    endResetModel();
    return !csvData.empty();
}

bool CSVEditorTable::save(const QString& path) const {
    std::ofstream file(path.toLocal8Bit());
    if (!file.is_open()) return false;
    for (const auto& row : csvData) {
        for (size_t i = 0; i < row.size(); ++i) {
            QString cell = row[i];
            if (cell.contains('"') || cell.contains(',')) {
                cell.replace("\"", "\"\"");
                cell = "\"" + cell + "\"";
            }
            file << cell.toStdString();
            if (i + 1 < row.size()) file << ",";
        }
        file << "\n";
    }
    return true;
}

int CSVEditorTable::rowCount(const QModelIndex&) const {
    return static_cast<int>(csvData.size());
}

int CSVEditorTable::columnCount(const QModelIndex&) const {
    size_t maxCols = 0;
    for (const auto& row : csvData) {
        if (row.size() > maxCols) maxCols = row.size();
    }
    return static_cast<int>(maxCols);
}

QVariant CSVEditorTable::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || role != Qt::DisplayRole && role != Qt::EditRole)
        return QVariant();
    size_t row = static_cast<size_t>(index.row());
    size_t col = static_cast<size_t>(index.column());
    if (row >= csvData.size() || col >= csvData[row].size())
        return QVariant();
    return csvData[row][col];
}

bool CSVEditorTable::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole)
        return false;
    size_t row = static_cast<size_t>(index.row());
    size_t col = static_cast<size_t>(index.column());
    if (row >= csvData.size())
        return false;
    if (col >= csvData[row].size())
        csvData[row].resize(col + 1);
    csvData[row][col] = value.toString();
    emit dataChanged(index, index, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

Qt::ItemFlags CSVEditorTable::flags(const QModelIndex& index) const {
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

QVariant CSVEditorTable::headerData(int section, Qt::Orientation orientation, int role) const {
    if (role != Qt::DisplayRole)
        return QVariant();
    if (orientation == Qt::Horizontal)
        return QString("Col %1").arg(section + 1);
    else
        return QString("Row %1").arg(section + 1);
}

bool CSVEditorTable::insertRows(int row, int count, const QModelIndex&) {
    if (row < 0 || row > static_cast<int>(csvData.size()) || count <= 0)
        return false;
    beginInsertRows(QModelIndex(), row, row + count - 1);
    size_t cols = colCountRaw();
    for (int i = 0; i < count; ++i)
        csvData.insert(csvData.begin() + row, std::vector<QString>(cols));
    endInsertRows();
    return true;
}

bool CSVEditorTable::removeRows(int row, int count, const QModelIndex&) {
    if (row < 0 || row + count > static_cast<int>(csvData.size()) || count <= 0)
        return false;
    beginRemoveRows(QModelIndex(), row, row + count - 1);
    csvData.erase(csvData.begin() + row, csvData.begin() + row + count);
    endRemoveRows();
    return true;
}

bool CSVEditorTable::insertColumns(int column, int count, const QModelIndex&) {
    if (column < 0 || count <= 0)
        return false;
    beginInsertColumns(QModelIndex(), column, column + count - 1);
    for (auto& row : csvData) {
        if (column > static_cast<int>(row.size()))
            row.resize(column, "");
        row.insert(row.begin() + column, count, "");
    }
    endInsertColumns();
    return true;
}

bool CSVEditorTable::removeColumns(int column, int count, const QModelIndex&) {
    if (column < 0 || count <= 0)
        return false;
    beginRemoveColumns(QModelIndex(), column, column + count - 1);
    for (auto& row : csvData) {
        if (column < static_cast<int>(row.size()))
            row.erase(row.begin() + column, row.begin() + std::min(row.size(), static_cast<size_t>(column + count)));
    }
    endRemoveColumns();
    return true;
}

// Convenience methods
size_t CSVEditorTable::rowCountRaw() const {
    return csvData.size();
}

size_t CSVEditorTable::colCountRaw() const {
    size_t maxCols = 0;
    for (const auto& row : csvData) {
        if (row.size() > maxCols) maxCols = row.size();
    }
    return maxCols;
}

std::optional<QString> CSVEditorTable::getCell(size_t row, size_t col) const {
    if (row >= csvData.size() || col >= csvData[row].size()) return std::nullopt;
    return csvData[row][col];
}

bool CSVEditorTable::setCell(size_t row, size_t col, const QString& value) {
    if (row >= csvData.size()) return false;
    if (col >= csvData[row].size()) csvData[row].resize(col + 1);
    csvData[row][col] = value;
    QModelIndex idx = index(static_cast<int>(row), static_cast<int>(col));
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    return true;
}

const std::vector<std::vector<QString>>& CSVEditorTable::dataRaw() const {
    return csvData;
}

} // namespace langscore