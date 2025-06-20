#pragma once

#include <QTableWidget>
#include <QLabel>
#include <QToolButton>
#include <vector>
#include <QString>

#include "ComponentBase.h"
#include "LoadFileManager.h"


struct ScriptTextData; // 前方宣言

class ScriptCSVTable : public QWidget, public ComponentBase {
    Q_OBJECT
public:
    ScriptCSVTable(ComponentBase* component, std::weak_ptr<LoadFileManager> loadFileManager, QWidget* parent = nullptr);
    
    void clear();

    void setupScriptTable();
    void filterScriptTextData(std::vector<ScriptTextData>& scriptDataList);

    void setScriptState(QString scriptName, Qt::CheckState check);

    void setScriptTableItemCheck(QTableWidgetItem* item, Qt::CheckState check);

    void unckeckSignOnlyText();
    void uncheckNotContainHiragana();

    void setTableItemTextColor(int row, QBrush color);

    void updateTableTextColor();

    QTableWidgetItem* scriptTableItem(int row, int col);

    QString getScriptFileNameFromTable(int row);

    std::vector<int> fetchScriptTableSameFileRows(QString scriptName);

    void scriptTableItemChanged(QTableWidgetItem* item);

    Qt::CheckState getTreeCheckStateBasedOnTable(QString scriptName);

    void showNormalJsonText(QString treeItemName, QString fileName);

    void setScriptFileName(QString fileName);

signals:
    void scriptTableSelected(QString scriptName, QString scriptFilePath, size_t textRow, size_t textCol, int textLen);

public slots:
    void changeScriptTableItemCheck(QString scriptName, Qt::CheckState);

private slots:
    void onScriptTableScrollToRow(const QString& scriptFileName);
    void onScriptTableSelectRow(const QString& scriptFileName);
    void onScriptTableSelected();

private:

    struct TableUndo : QUndoCommand
    {
        using ValueType = bool;
        TableUndo(ScriptCSVTable* parent, QTableWidgetItem* target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue)){
        }
        ~TableUndo() {}

        int id() const override { return 2; }
        void undo() override;
        void redo() override;

    private:
        ScriptCSVTable* parent;
        QTableWidgetItem* target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };


    void writeToIgnoreScriptLine(int row, bool ignore);
    void updateScriptWordCount(QString text, Qt::CheckState state);


    bool showAllScriptContents;
    int currentScriptWordCount;
    QLabel* scriptFileName;
    QLabel* scriptFileWordCount;
    QToolButton* autoCheckButton;
    QToolButton* scriptFilterButton;
    QTableWidget* tableWidget;
    std::weak_ptr<LoadFileManager> loadFileManager;

    //fetchScriptTableSameFileRowsの高速化のためのキャッシュ
    std::unordered_map<QString, std::vector<int>> scriptNameToTableIndexMap;

};