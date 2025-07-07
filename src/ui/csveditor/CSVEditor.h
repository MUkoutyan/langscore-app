#pragma once

#include <QWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QStringList>
#include <vector>
#include "CSVEditorTable.h"
#include "FastCSVContainer.h"


class CSVEditor : public QTableView {
    Q_OBJECT
public:
    explicit CSVEditor(QWidget* parent = nullptr);

    //bool openCSV(const QString& filePath);
    //bool saveCSV(const QString& filePath);

};