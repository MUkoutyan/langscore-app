#include "ScriptCSVTableModel.h"


int ScriptCSVTableModel::rowCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.rowCount());
}

int ScriptCSVTableModel::columnCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.columnCount());
}

QVariant ScriptCSVTableModel::data(const QModelIndex& index, int role) const
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

QVariant ScriptCSVTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }

    if(orientation == Qt::Horizontal && section >= 0 && section < static_cast<int>(csvContainer.columnCount())) {
        return csvContainer.headerAt(static_cast<size_t>(section));
    }

    return QVariant();
}

Qt::ItemFlags ScriptCSVTableModel::flags(const QModelIndex& index) const
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

bool ScriptCSVTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
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