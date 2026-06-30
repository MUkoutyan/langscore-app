#pragma once

#include <QTableView>
#include <QLabel>
#include <QToolButton>
#include <QPushButton>
#include <QLineEdit>
#include <vector>
#include <QString>
#include <QPersistentModelIndex>

#include "ComponentBase.h"
#include "CSVEditDataManager.h"
#include "csveditor/CSVEditorSortFilterProxyModel.h"
#include "ScriptTableViewModel.h"


struct ScriptTextData; // 前方宣言

class ScriptCSVTable : public QWidget, public ComponentBase {
    Q_OBJECT
public:
    ScriptCSVTable(ComponentBase* component, std::weak_ptr<CSVEditDataManager> loadFileManager, QWidget* parent = nullptr);

    void clear();

    void setupScriptTable();
    void filterScriptTextData(std::vector<ScriptTextData>& scriptDataList);

    void updateScriptIgnoreState(QString scriptName, Qt::CheckState check);

    void unckeckSignOnlyText();
    void uncheckNotContainHiragana();

    void updateTableTextColor();

    QString getScriptFileNameFromTable(int sourceRow);

    std::vector<int> fetchScriptTableSameFileRows(QString scriptName);

    Qt::CheckState getTreeCheckStateBasedOnTable(QString scriptName);

    void setScriptFileName(QString fileName);

signals:
    void scriptTableSelected(QString scriptName, QString scriptFilePath, size_t textRow, size_t textCol, int textLen);
    void notifyScriptTableChangeItemCheck(QString fileName, Qt::CheckState);

public slots:
    void changeScriptTableItemCheck(QString scriptName, Qt::CheckState);

private slots:
    void onScriptTableScrollToRow(const QString& scriptFileName);
    void onScriptTableSelectRow(const QString& scriptFileName);
    void onScriptTableSelected();
    void onContextMenuRequested(const QPoint& pos);

private:

    struct TableUndo : QUndoCommand
    {
        using ValueType = Qt::CheckState;
        TableUndo(ScriptTableViewModel* model, const QPersistentModelIndex& target,
                  ValueType newValue, ValueType oldValue)
            : model(model), target(target), newValue(newValue), oldValue(oldValue) {}
        ~TableUndo() {}

        int id() const override { return 2; }
        void undo() override;
        void redo() override;

    private:
        ScriptTableViewModel* model;
        QPersistentModelIndex target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };

    void receive(DispatchType type, const QVariantList& args) override;
    void restoreColumnWidths();
    void showLanguageColumnMenu();

    bool showAllScriptContents;
    QLabel*    scriptFileName;
    QLabel*    scriptFileWordCount;
    QToolButton* autoCheckButton;
    QToolButton* scriptFilterButton;
    QToolButton* settingButton;
    QWidget*     settingPane;
    QPushButton* hideLanguageColumnsAction;
    QLineEdit*   filterEdit;
    QTableView*  tableView;
    ScriptTableViewModel*                      currentModel;
    langscore::CSVEditorSortFilterProxyModel*  _proxyModel;
    std::weak_ptr<CSVEditDataManager> loadFileManager;

};