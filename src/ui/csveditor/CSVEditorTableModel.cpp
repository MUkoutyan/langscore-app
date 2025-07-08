#include "CSVEditorTableModel.h"
#include "../../csv.hpp"
#include "../../settings.h"
#include "../../utility.hpp"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <fstream>

namespace langscore {

CSVEditorTableModel::CSVEditorTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

CSVEditorTableModel::CSVEditorTableModel(const QString& path, QObject* parent)
    : QAbstractTableModel(parent)
{
    loadFromFile(path);
}

bool CSVEditorTableModel::loadFromFile(const QString& path) {
    beginResetModel();
    bool success = csvContainer.loadFromFile(path);
    endResetModel();
    return success;
}


bool CSVEditorTableModel::loadFromJsonFile(const QString& filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << filePath;
        return false;
    }

    QJsonArray jsonArray = jsonDoc.array();
    if(jsonArray.isEmpty()) {
        qWarning() << "JSON array is empty in file:" << filePath;
        return false;
    }

    beginResetModel();

    // コンテナをクリア
    csvContainer.clear();

    // ヘッダーを設定
    std::vector<std::vector<QString>> csvData;
    csvData.push_back({tr("Original"), tr("Type")}); // ヘッダー行

    // データ行を処理
    int wordCount_ = 0;
    for(int r = 0; r < jsonArray.size(); ++r) {
        QJsonObject item = jsonArray.at(r).toObject();
        QString originalText = item["original"].toString();
        wordCount_ += langscore::wordCountUTF8(originalText);

        QJsonArray typeArray = item["type"].toArray();
        QStringList typeList;
        for(const QJsonValue& typeValue : typeArray) {
            typeList.append(typeValue.toString());
        }
        QString typeText = typeList.join(", ");

        csvData.push_back({originalText, typeText});
    }

    // FastCSVContainerにデータを読み込み
    bool success = csvContainer.loadFromCsvData(csvData);

    endResetModel();
    return success;
}

bool CSVEditorTableModel::saveToFile(const QString& path) const
{
    return csvContainer.saveToFile(path);
}

int CSVEditorTableModel::rowCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.rowCount());
}

int CSVEditorTableModel::columnCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.columnCount());
}

QVariant CSVEditorTableModel::data(const QModelIndex& index, int role) const {
    if(!index.isValid() || (role != Qt::DisplayRole && role != Qt::EditRole)) {
        return QVariant();
    }

    int row = index.row();
    int col = index.column();

    QString value;
    if(csvContainer.getValue(static_cast<size_t>(row), static_cast<size_t>(col), value)) {
        return value;
    }

    return QVariant();
}

bool CSVEditorTableModel::setData(const QModelIndex& index, const QVariant& value, int role) 
{
    if(role != Qt::EditRole || !index.isValid()) {
        return false;
    }

    int row = index.row();
    int col = index.column();

    if(csvContainer.setValue(static_cast<size_t>(row), static_cast<size_t>(col), value.toString())) {
        emit dataChanged(index, index);
        return true;
    }

    return false;
}

Qt::ItemFlags CSVEditorTableModel::flags(const QModelIndex& index) const 
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    // ヘッダーの内容を参照して、originalとtype以外の列は編集可能にする
    int col = index.column();
    auto headerText = headerData(col, Qt::Horizontal, Qt::DisplayRole);
    if(headerText == tr("Original") || headerText == tr("Type")) {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled; // 編集不可
    }
    
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

QVariant CSVEditorTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }

    if(orientation == Qt::Horizontal && section >= 0 && section < static_cast<int>(csvContainer.columnCount())) {
        return csvContainer.headerAt(static_cast<size_t>(section));
    }
    return QVariant();
}

// Convenience methods
size_t CSVEditorTableModel::rowCountRaw() const {
    return csvContainer.rowCount();
}

size_t CSVEditorTableModel::colCountRaw() const {
    return csvContainer.columnCount();
}

std::optional<QString> CSVEditorTableModel::getCell(size_t row, size_t col) const 
{
    if(row >= csvContainer.rowCount() || col >= csvContainer.columnCount()) { return std::nullopt; }
    return csvContainer[row][col];
}

bool CSVEditorTableModel::setCell(size_t row, size_t col, const QString& value) 
{
    if(row >= csvContainer.rowCount()) { return false; }

    csvContainer[row][col] = value;
    QModelIndex idx = index(static_cast<int>(row), static_cast<int>(col));
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    return true;
}


void CSVEditorTableModel::insertColumn(size_t index, const QString& columnName)
{
    beginResetModel();
    csvContainer.insertColumn(index, columnName, "");
    endResetModel();
}

void CSVEditorTableModel::removeColumn(int column)
{
    if(column < 0 || column >= static_cast<int>(csvContainer.columnCount())) {
        return;
    }

    beginResetModel();
    csvContainer.removeColumn(static_cast<size_t>(column));
    endResetModel();
}


const std::vector<std::vector<QString>>& CSVEditorTableModel::dataRaw() const {
    return csvContainer.dataRaw();
}

} // namespace langscore