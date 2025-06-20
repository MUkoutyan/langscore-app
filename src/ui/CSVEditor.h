#pragma once

#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QStringList>
#include <vector>
#include "CSVEditorTable.h"

class CSVTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit CSVTableModel(QObject* parent = nullptr);

    bool loadFromFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

private:
    std::vector<QString> header;
    std::vector<std::vector<QString>> contents;
};

class CSVEditor : public QWidget {
    Q_OBJECT
public:
    explicit CSVEditor(QWidget* parent = nullptr);

    bool openCSV(const QString& filePath);
    bool saveCSV(const QString& filePath);

private:
    QTableView* tableView;
    CSVTableModel* model;
};