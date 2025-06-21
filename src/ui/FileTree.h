#pragma once

#include <QTreeWidget>
#include <QObject>
#include "ComponentBase.h"
#include "LoadFileManager.h"
#include "GraphicsImageLoader.h"

class QLineEdit;
class QToolButton;
class QHBoxLayout;
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


    struct FileTreeUndo : QUndoCommand
    {
        using ValueType = Qt::CheckState;
        FileTreeUndo(FileTree* parent, QTreeWidgetItem* target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue))
        {
        }
        ~FileTreeUndo() {}

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
    ~FileTree();

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
    void setupSearchBar();
    void filterTree(const QString& searchText);
    void resetTreeFilter();
    bool filterTreeItem(QTreeWidgetItem* item, const QString& searchText, bool parentMatched = false);
    void expandItemsWithChildren(QTreeWidgetItem* item);
    void resetItemVisibility(QTreeWidgetItem* item);

    QLineEdit* searchLineEdit = nullptr;
    QHBoxLayout* searchLayout = nullptr;
    bool isFiltering = false;

    std::weak_ptr<LoadFileManager> loadFileManager;
    QTreeWidget* treeWidget;
    bool _suspendHistory;

    GraphicsImageLoader* graphicsImageLoader = nullptr;
    QThread* graphicsLoaderThread = nullptr;
    QMap<QString, QTreeWidgetItem*> graphicsItemMap; // filePath -> item

private slots:
    void onGraphicsImageLoaded(const QString& filePath, QTreeWidgetItem* item, int column, const QImage& image);
};