#pragma once

#include <QTableWidget>
#include <QLabel>
#include <QToolButton>
#include <QString>

#include <QTextEdit>
#include <vector>

#include "ComponentBase.h"
#include "EditDataManager.h"
#include "editor/CSVEditor.h"


struct ScriptTextData; // 前方宣言
class MainCSVTableModel;
class invoker;
class MainCSVTable : public QWidget, public ComponentBase {
    Q_OBJECT
public:
    MainCSVTable(ComponentBase* component, std::weak_ptr<EditDataManager> loadFileManager, QWidget* parent = nullptr);
    
    void clear();

    void showMainFileText(QString treeItemName, QString fileName);

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

    void receive(DispatchType type, const QVariantList& args) override;
    void restoreColumnWidths();

    std::weak_ptr<EditDataManager> loadFileManager;
    QLabel* mainFileName;
    QLabel* mainFileWordCount;
    QToolButton* filterMenuButton;
    QAction* showAllAction;
    QAction* hideTranslatedAction;
    QPushButton* hideLanguageColumnsAction;
    QPushButton* validateButton;
    QPushButton* validateResultListButton;
    QDialog* validateResultDialog;
    QTableWidget* validateResultTable;
    QToolButton* settingButton;
    QWidget* settingPane;
    MainCSVTableModel* currentModel;
    CSVEditor* csvEditor;
    langscore::CSVEditorSortFilterProxyModel* _proxyModel = nullptr;
    QTextEdit* cellErrorLog;
    invoker* _invoker;
    QTimer* updateTimer;
    QString currentFileName;
    bool _finishInvoke;
    bool _showAllScriptContents;
};