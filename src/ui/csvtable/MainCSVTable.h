#pragma once

#include <QTableWidget>
#include <QLabel>
#include <QToolButton>
#include <vector>
#include <QString>

#include "ComponentBase.h"
#include "CSVEditDataManager.h"
#include "csveditor/CSVEditor.h"


struct ScriptTextData; // 前方宣言
class MainCSVTableModel;
class MainCSVTable : public QWidget, public ComponentBase {
    Q_OBJECT
public:
    MainCSVTable(ComponentBase* component, std::weak_ptr<CSVEditDataManager> loadFileManager, QWidget* parent = nullptr);
    
    void clear();

    void setupTable();

    void setTableItemTextColor(int row, QBrush color);

    QModelIndex scriptTableItem(int row, int col);

    QString getScriptFileNameFromTable(int row);


    void scriptTableItemChanged(QModelIndex item);


    void showMainFileText(QString treeItemName, QString fileName);

    void setScriptFileName(QString fileName);

public slots:
    void changeScriptTableItemCheck(QString scriptName, Qt::CheckState);

private:

    struct TableUndo : QUndoCommand
    {
        using ValueType = bool;
        TableUndo(MainCSVTable* parent, QModelIndex target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue)){
        }
        ~TableUndo() {}

        int id() const override { return 6; }
        void undo() override;
        void redo() override;

    private:
        MainCSVTable* parent;
        QModelIndex target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };


    std::vector<int> fetchTableSameFileRows(QString mainFileName);
    void receive(DispatchType type, const QVariantList& args) override;

    std::weak_ptr<CSVEditDataManager> loadFileManager;
    QLabel* mainFileName;
    QLabel* mainFileWordCount;
    QToolButton* settingButton;
    QWidget* settingPane;
    MainCSVTableModel* currentModel;
    CSVEditor* csvEditor;


};