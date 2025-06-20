#include "CSVEditor.h"
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include "../csv.hpp"
#include "CSVEditorTable.h"

CSVTableModel::CSVTableModel(QObject* parent)
    : QAbstractTableModel(parent) {}

bool CSVTableModel::loadFromFile(const QString& filePath) 
{
    auto csv = langscore::readCsv(filePath);
    if(csv.empty()) { return false; }

    this->header = csv[0];
    std::copy(csv.begin() + 1, csv.end(), std::back_inserter(this->contents));

    beginResetModel();
    endResetModel();
    return true;
}

bool CSVTableModel::saveToFile(const QString& filePath) const 
{
    //QFile file(filePath);
    //if(file.open(QIODevice::WriteOnly | QIODevice::Text) == false) {
    //    return false;
    //}

    //QTextStream out(&file);
    //out << header.join(',') << "\n";
    //for (const auto& row : contents) {
    //    QStringList fields;
    //    for (const auto& cell : row)
    //        fields << cell;
    //    out << fields.join(',') << "\n";
    //}
    //file.close();

    return true;
}

int CSVTableModel::rowCount(const QModelIndex&) const {
    return static_cast<int>(contents.size());
}

int CSVTableModel::columnCount(const QModelIndex&) const {
    return static_cast<int>(header.size());
}

QVariant CSVTableModel::data(const QModelIndex& index, int role) const {
    if(index.isValid() == false || (role != Qt::DisplayRole && role != Qt::EditRole)) {
        return QVariant();
    }
    int row = index.row();
    int col = index.column();
    if(row < 0 || row >= static_cast<int>(contents.size()) || col < 0 || col >= header.size()) {
        return QVariant();
    }
    return contents[row][col];
}

QVariant CSVTableModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if(role != Qt::DisplayRole) {
        return QVariant();
    }
    if(orientation == Qt::Horizontal && section < header.size()) {
        return header[section];
    }
    return QVariant();
}

Qt::ItemFlags CSVTableModel::flags(const QModelIndex& index) const {
    if(index.isValid() == false) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

bool CSVTableModel::setData(const QModelIndex& index, const QVariant& value, int role) 
{
    if(role != Qt::EditRole || index.isValid() == false) {
        return false;
    }

    int row = index.row();
    int col = index.column();
    if(row < 0 || row >= static_cast<int>(contents.size()) || col < 0 || col >= header.size()) {
        return false;
    }
    contents[row][col] = value.toString();
    emit dataChanged(index, index);
    return true;
}

// CSVEditor

CSVEditor::CSVEditor(QWidget* parent)
    : QWidget(parent), tableView(new QTableView(this)), model(new CSVTableModel(this)) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(tableView);
    tableView->setModel(model);
    setLayout(layout);
}

bool CSVEditor::openCSV(const QString& filePath) {
    return model->loadFromFile(filePath);
}

bool CSVEditor::saveCSV(const QString& filePath) {
    return model->saveToFile(filePath);
}
