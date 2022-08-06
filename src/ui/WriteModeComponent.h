﻿#pragma once

#include <QWidget>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QGraphicsPixmapItem>
#include "ComponentBase.h"

#include <variant>

namespace Ui {
class WriteModeComponent;
}

class LanguageSelectComponent;
class MainComponent;
class WriteModeComponent : public QWidget, public ComponentBase
{
    Q_OBJECT
public:
    explicit WriteModeComponent(ComponentBase* setting, MainComponent *parent);
    ~WriteModeComponent() override;

    void show();
    void clear();

signals:

public slots:
    void treeItemSelected();
    void treeItemChanged(QTreeWidgetItem *_item, int column);
    void scriptTableSelected();
    void scriptTableItemChanged(QTableWidgetItem *item);

    void exportTranslateFiles();

private:
    Ui::WriteModeComponent* ui;
    MainComponent* _parent;
    QGraphicsScene* scene;
    std::vector<LanguageSelectComponent*> languageButtons;
    bool showAllScriptContents;
    bool _suspendHistory;

    void initializeScriptCsvText();
    void initializeTree();

    void showNormalCsvText(QString fileName);
    void setTreeItemCheck(QTreeWidgetItem *_item, Qt::CheckState check);
    void setScriptTableItemCheck(QTableWidgetItem *_item, Qt::CheckState check);

    void writeToScriptListSetting(QTreeWidgetItem *item, bool ignore);
    void writeToIgnoreScriptLine(int row, bool ignore);
    void writeToPictureListSetting(QTreeWidgetItem *item, bool ignore);

    QTableWidgetItem* scriptTableItem(int row, int col);

    std::vector<int> fetchScriptTableSameFileRows(QString fileName);
    QTreeWidgetItem* fetchScriptTreeSameFileItems(QString fileName);
    void setTableItemTextColor(int row, QBrush color);

    Qt::CheckState getTreeCheckStateBasedOnTable(QString itemFileName);

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

};

