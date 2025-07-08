#pragma once

#include <QAbstractTableModel>
#include "CSVEditorTableModel.h"

class MainCSVTableModel : public langscore::CSVEditorTableModel {
    Q_OBJECT
public:
    explicit MainCSVTableModel(QObject* parent = nullptr);

private:
};
