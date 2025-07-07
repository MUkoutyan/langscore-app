#pragma once

#include <QAbstractTableModel>
#include "FastCSVContainer.h"

class MainCSVTableModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit MainCSVTableModel(QObject* parent = nullptr);

    bool loadFromFile(const QString& filePath);
    bool loadFromJsonFile(const QString& filePath);
    bool saveToFile(const QString& filePath) const;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;

    void addColumn(const QString& columnName);
    void insertColumn(size_t index, const QString& columnName);
    void removeColumn(int column);


    void clearAll() {
        beginResetModel();
        csvContainer.clear();
        endResetModel();
    }


private:
    langscore::FastCSVContainer csvContainer;
};
