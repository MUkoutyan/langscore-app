#pragma once

#include <QTreeWidget>
#include <QObject>
#include "ComponentBase.h"
#include "CSVEditDataManager.h"
#include "service/GraphicsImageLoader.h"
#include "service/ValidationErrorInfo.h"

#include <mutex>

class QLineEdit;
class QToolButton;
class QHBoxLayout;
class QTimer;
class invoker;
class FileTree : public QWidget, public ComponentBase
{
    Q_OBJECT
public:

    enum TreeItemType {
        Basic,
        Map,
        Script,
        Pictures,
    };

    enum TreeColIndex {
        CheckBox = 0,
        Name = 0,
        NumColumn
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


    FileTree(ComponentBase* component, std::weak_ptr<CSVEditDataManager>, QWidget* parent = nullptr);
    ~FileTree();

    void clear();

    void setupTree();
    void setupBasicsTree();
    void setupMainTree();
    void setupScriptTree();
    void setupGraphicsTree();

    void itemSelected();
    void itemChanged(QTreeWidgetItem* _item, int column);

    void setTreeItemCheck(QTreeWidgetItem* _item, Qt::CheckState check);

    void writeToBasicDataListSetting(QTreeWidgetItem* item, bool ignore);
    void writeToMapDataListSetting(QTreeWidgetItem* item, bool ignore);
    void writeToScriptListSetting(QTreeWidgetItem* item, bool ignore);
    void writeToPictureListSetting(QTreeWidgetItem* item, bool ignore);

    void onChangeScriptTableItemCheck(QString scriptName, Qt::CheckState);

    QTreeWidgetItem* fetchScriptTreeSameFileItem(QString scriptName);

    void updateTreeTextColor();
    void updateTreeVisibility();

    void clearErrors();

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
    void notifyScriptTreeItemCheckChanged(QString scriptName, Qt::CheckState check);

private:
    void setupSearchBar();
    void filterTree(const QString& searchText);
    void resetTreeFilter();
    bool filterTreeItem(QTreeWidgetItem* item, const QString& searchText, bool parentMatched = false);
    void expandItemsWithChildren(QTreeWidgetItem* item);
    void resetItemVisibility(QTreeWidgetItem* item);

    void receive(DispatchType type, const QVariantList& args) override;

    invoker* _invoker;

    QLineEdit* searchLineEdit = nullptr;
    QHBoxLayout* searchLayout = nullptr;
    bool isFiltering = false;

    // 以下を追加
    QToolButton* settingButton = nullptr;
    QWidget* settingPane = nullptr;
    QCheckBox* showHiddenCheckBox = nullptr;

    std::weak_ptr<CSVEditDataManager> loadFileManager;
    QTreeWidget* treeWidget;
    bool _suspendHistory;

    GraphicsImageLoader* graphicsImageLoader = nullptr;
    QThread* graphicsLoaderThread = nullptr;
    QMap<QString, QTreeWidgetItem*> graphicsItemMap; // filePath -> item


    std::unordered_map<QString, QTreeWidgetItem*> treeTopItemList;
    size_t errorInfoIndex;

    std::mutex errorMutex;
    QIcon attentionIcon;
    QIcon warningIcon;


    std::vector<ValidationErrorInfo> processJsonBuffer(const QString& input);
    QTreeWidgetItem* findTreeItemByFileName(const QString& fileName);
    QTreeWidgetItem* findTreeItemRecursive(QTreeWidgetItem* parent, const QString& fileName);
    void clearErrorItemsRecursive(QTreeWidgetItem* parent);

    //キーはファイルパス
    ValidationErrorInfo convertErrorInfo(std::vector<QString> csvText);

private slots:
    void onGraphicsImageLoaded(const QString& filePath, QTreeWidgetItem* item, int column, const QImage& image);
    void showContextMenu(const QPoint& position);
    void toggleItemVisibility(QTreeWidgetItem* targetItem); // hideSelectedItem をこちらに変更します
    void toggleShowHiddenFiles(bool isChecked);
    
    //検証で使用するメソッド
    void addErrorText(QString text);
    void updateErrorTree();
};