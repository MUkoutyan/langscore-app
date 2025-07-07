#include "MainCSVTableModel.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "../../settings.h"
#include "../../utility.hpp"

MainCSVTableModel::MainCSVTableModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

bool MainCSVTableModel::loadFromFile(const QString& filePath)
{
    beginResetModel();
    bool success = csvContainer.loadFromFile(filePath);
    endResetModel();
    return success;
}


bool MainCSVTableModel::loadFromJsonFile(const QString& filePath)
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


bool MainCSVTableModel::saveToFile(const QString& filePath) const
{
    return csvContainer.saveToFile(filePath);
}

int MainCSVTableModel::rowCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.rowCount());
}

int MainCSVTableModel::columnCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.columnCount());
}

QVariant MainCSVTableModel::data(const QModelIndex& index, int role) const
{
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

QVariant MainCSVTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }

    if(orientation == Qt::Horizontal && section >= 0 && section < static_cast<int>(csvContainer.columnCount())) {
        return csvContainer.headerAt(static_cast<size_t>(section));
    }

    return QVariant();
}

Qt::ItemFlags MainCSVTableModel::flags(const QModelIndex& index) const
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

bool MainCSVTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
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

void MainCSVTableModel::addColumn(const QString& columnName)
{
    beginResetModel();
    csvContainer.addColumn(columnName, "");
    endResetModel();
}

void MainCSVTableModel::insertColumn(size_t index, const QString& columnName)
{
    beginResetModel();
    csvContainer.insertColumn(index, columnName, "");
    endResetModel();
}

void MainCSVTableModel::removeColumn(int column)
{
    if(column < 0 || column >= static_cast<int>(csvContainer.columnCount())) {
        return;
    }

    beginResetModel();
    csvContainer.removeColumn(static_cast<size_t>(column));
    endResetModel();
}
