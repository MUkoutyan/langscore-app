#pragma once

#include <QTableWidget>
#include <QLabel>
#include <QToolButton>
#include <vector>
#include <QString>

#include "ComponentBase.h"
#include "LoadFileManager.h"


struct ScriptTextData; // 前方宣言

class MainCSVTable : public QWidget, public ComponentBase {
    Q_OBJECT
public:
    MainCSVTable(ComponentBase* component, std::weak_ptr<LoadFileManager> loadFileManager, QWidget* parent = nullptr);
    
    void clear();

    void setupTable();

    void setTableItemTextColor(int row, QBrush color);

    QTableWidgetItem* scriptTableItem(int row, int col);

    QString getScriptFileNameFromTable(int row);


    void scriptTableItemChanged(QTableWidgetItem* item);


    void showMainFileText(QString treeItemName, QString fileName);

    void setScriptFileName(QString fileName);

signals:
    void scriptTableSelected(QString scriptName, QString scriptFilePath, size_t textRow, size_t textCol, int textLen);

public slots:
    void changeScriptTableItemCheck(QString scriptName, Qt::CheckState);

private slots:
    void onTableScrollToRow(const QString& scriptFileName);
    void onTableSelectRow(const QString& scriptFileName);
    void onTableSelected();

private:

    struct TableUndo : QUndoCommand
    {
        using ValueType = bool;
        TableUndo(MainCSVTable* parent, QTableWidgetItem* target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue)){
        }
        ~TableUndo() {}

        int id() const override { return 6; }
        void undo() override;
        void redo() override;

    private:
        MainCSVTable* parent;
        QTableWidgetItem* target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };


    void updateWordCount(QString text, Qt::CheckState state);
    std::vector<int> fetchTableSameFileRows(QString mainFileName);

    int currentWordCount;
    QLabel* mainFileName;
    QLabel* mainFileWordCount;
    QTableWidget* tableWidget;
    std::weak_ptr<LoadFileManager> loadFileManager;

};