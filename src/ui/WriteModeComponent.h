#pragma once

#include <QWidget>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QGraphicsPixmapItem>

#include <QPainter>
#include <QResizeEvent>

#include "ComponentBase.h"

#include <variant>

namespace Ui {
class WriteModeComponent;
}

class ScriptViewer;
class LanguageSelectComponent;
class invoker;
class WriteModeComponent : public QWidget, public ComponentBase
{
    Q_OBJECT
public:
    explicit WriteModeComponent(ComponentBase* setting, QWidget *parent);
    ~WriteModeComponent() override;

    void show();
    void clear();

signals:
    void toNextPage();

public slots:
    void treeItemSelected();
    void treeItemChanged(QTreeWidgetItem *_item, int column);
    void scriptTableSelected();
    void scriptTableItemChanged(QTableWidgetItem *item);

    void exportPlugin();
    void exportTranslateFiles();
    void firstExportTranslateFiles();

private:

    enum InvokeType {
        None,
        ExportFirstTime,
        ExportCSV,
        UpdatePlugin,
        Reanalisys
    };

    Ui::WriteModeComponent* ui;
    QGraphicsScene* scene;
    std::vector<LanguageSelectComponent*> languageButtons;
    std::vector<std::pair<QString, QString>> scriptFileNameMap;
    int currentScriptWordCount;
    bool showAllScriptContents;
    bool _suspendHistory;
    invoker* _invoker;
    InvokeType invokeType;
    QString lastWritePath;
    ScriptViewer* scriptViewer;

    //fetchScriptTableSameFileRowsの高速化のためのキャッシュ
    std::unordered_map<QString, std::vector<int>> scriptNameToTableIndexMap;

    std::unordered_map<Qt::CheckState, QColor> tableTextColorForState;

    void setup();
    void setupScriptCsvText();
    void setupTree();

    void showNormalJsonText(QString treeItemName, QString fileName);
    void setTreeItemCheck(QTreeWidgetItem *_item, Qt::CheckState check);
    void setScriptTableItemCheck(QTableWidgetItem *_item, Qt::CheckState check);

    void writeToBasicDataListSetting(QTreeWidgetItem *item, bool ignore);
    void writeToScriptListSetting(QTreeWidgetItem *item, bool ignore);
    void writeToIgnoreScriptLine(int row, bool ignore);
    void writeToPictureListSetting(QTreeWidgetItem *item, bool ignore);

    std::vector<int> fetchScriptTableSameFileRows(QString scriptName);
    QTreeWidgetItem* fetchScriptTreeSameFileItem(QString scriptName);
    void setTableItemTextColor(int row, QBrush color);

    Qt::CheckState getTreeCheckStateBasedOnTable(QString scriptName);

    void updateScriptWordCount(QString text, Qt::CheckState state);
    void backup();

    QTableWidgetItem* scriptTableItem(int row, int col);
    QString getScriptName(QString fileName);
    QString getScriptFileName(QString scriptName);
    QString getScriptFileNameFromTable(int row);

    void setFontList(std::vector<QString> fontPaths);

    void changeEnabledUIState(bool enable);

    void receive(DispatchType type, const QVariantList& args) override;

    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    struct TableUndo : QUndoCommand
    {
        using ValueType = bool;
        TableUndo(WriteModeComponent* parent, QTableWidgetItem* target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue))
        {}
        ~TableUndo(){}

        int id() const override { return 2; }
        void undo() override;
        void redo() override;

    private:
        WriteModeComponent* parent;
        QTableWidgetItem* target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };

    struct TreeUndo : QUndoCommand
    {
        using ValueType = Qt::CheckState;
        TreeUndo(WriteModeComponent* parent, QTreeWidgetItem* target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue))
        {}
        ~TreeUndo(){}

        int id() const override { return 3; }
        void undo() override;
        void redo() override;

    private:
        WriteModeComponent* parent;
        QTreeWidgetItem* target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif

    void changeUIColor();
};

