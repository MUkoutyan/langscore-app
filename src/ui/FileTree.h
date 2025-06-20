#pragma once

#include <QTreeWidget>
#include <QObject>
#include "ComponentBase.h"
#include "LoadFileManager.h"

class FileTree : public QWidget, public ComponentBase
{
    Q_OBJECT
public:

    enum TreeItemType {
        Main,
        Script,
        Pictures
    };

    enum TreeColIndex {
        CheckBox = 0,
        Name
    };


    struct TreeUndo : QUndoCommand
    {
        using ValueType = Qt::CheckState;
        TreeUndo(FileTree* parent, QTreeWidgetItem* target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue))
        {
        }
        ~TreeUndo() {}

        int id() const override { return 3; }
        void undo() override;
        void redo() override;

    private:
        FileTree* parent;
        QTreeWidgetItem* target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };


    FileTree(ComponentBase* component, std::weak_ptr<LoadFileManager>, QWidget* parent = nullptr);

    void clear();

    void setupTree();
    void setupMainTree();
    void setupScriptTree();
    void setupGraphicsTree();

    void itemSelected();
    void itemChanged(QTreeWidgetItem* _item, int column);

    void setTreeItemCheck(QTreeWidgetItem* _item, Qt::CheckState check);

    void writeToBasicDataListSetting(QTreeWidgetItem* item, bool ignore);
    void writeToScriptListSetting(QTreeWidgetItem* item, bool ignore);
    void writeToPictureListSetting(QTreeWidgetItem* item, bool ignore);

    void onChangeScriptTableItemCheck(QString scriptName, Qt::CheckState);

    QTreeWidgetItem* fetchScriptTreeSameFileItem(QString scriptName);

    void updateTreeTextColor();

signals:
    // Scriptタブでスクリプトファイル名に該当する行をスクロール・選択する
    void scriptTableScrollToRow(const QString& scriptFileName);
    void scriptTableSelectRow(const QString& scriptFileName);

    // Graphicsタブで画像を表示する
    void showGraphicsImage(const QString& imagePath);

    // MainタブでJSONを表示する
    void showMainFile(const QString& treeItemName, const QString& fileName);

    // Scriptタブでスクリプトファイルを表示する
    void showScriptFile(const QString& scriptFilePath);

    // Scriptタブでスクリプトファイル名をUIに表示する
    void setScriptFileNameLabel(const QString& label);

    // Scriptタブでスクリプトのハイライト
    void highlightScriptText(int row, int col, int length);

    // タブ切り替え
    void setTabIndex(int index);

    // その他必要なsignalを追加
    void scriptTreeItemCheckChanged(QString scriptName, Qt::CheckState check);

private:

    void itemSelectedMain();
    void itemSelectedScript();
    void itemSelectedGraphics();

    std::weak_ptr<LoadFileManager> loadFileManager;
    QTreeWidget* treeWidget;
    bool _suspendHistory;
};