#include "CSVEditor.h"
#include "MainCSVTableModel.h"
#include "MultiLineEditDelegate.h"
#include <QVBoxLayout>
#include <QFile>
#include "CSVEditorTable.h"
#include "FastCSVContainer.h"

// CSVEditor

CSVEditor::CSVEditor(QWidget* parent)
    : QTableView(parent)
{
    this->setSelectionBehavior(QAbstractItemView::SelectItems);

    // マルチライン編集デリゲートを設定
    auto* multiLineDelegate = new MultiLineEditDelegate(this);
    this->setItemDelegate(multiLineDelegate);
}

//bool CSVEditor::openCSV(const QString& filePath) {
//    return model->loadFromFile(filePath);
//}
//
//bool CSVEditor::saveCSV(const QString& filePath) {
//    //return model->saveToFile(filePath);
//}