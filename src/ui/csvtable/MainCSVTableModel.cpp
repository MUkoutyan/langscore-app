#include "MainCSVTableModel.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

#include "../../settings.h"
#include "../../utility.hpp"

MainCSVTableModel::MainCSVTableModel(QObject* parent)
    : CSVEditorTableModel(parent) {
}