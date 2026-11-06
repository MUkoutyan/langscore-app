#pragma once

#include <QTreeWidget>
#include <QObject>
#include <QCheckBox>
#include "ComponentBase.h"
#include "EditDataManager.h"
#include "service/GraphicsImageLoader.h"
#include "service/ValidationErrorInfo.h"

#include <mutex>

class QLineEdit;
namespace langscore { class FastCSVContainer; }
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
        Progress = 1,
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


    FileTree(ComponentBase* component, std::weak_ptr<EditDataManager>, QWidget* parent = nullptr);
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
    //ファイルの翻訳の進捗を表示する（fileName は拡張子付きでもよい）
    void setFileProgress(const QString& fileName, int translatedRows, int totalRows);

signals:
    // Scriptタブでスクリプトファイル名に該当する行をスクロール・選択する
    void scriptTableScrollToRow(const QString& scriptFileName);
    void scriptTableSelectRow(const QString& scriptFileName);

    // Graphicsタブで画像を表示する
    void showGraphicsImage(const QString& imagePath);

    // MainタブでJSONを表示する
    void showMainFile(const QString& treeItemName, const QString& fileName);
    // Mainタブで検証結果（ValidationErrorInfo::id）のセルを選択する
    void selectMainError(quint64 errorId);
    //スクリプトの検証結果を選んだとき。スクリプトの表で、そのスクリプトの原文の行を選ぶ
    void selectScriptError(const QString& scriptFileName, const QString& original);

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
    void updateTreeItemText();

    void receive(DispatchType type, const QVariantList& args) override;

    invoker* _invoker;

    QLineEdit* searchLineEdit = nullptr;
    QHBoxLayout* searchLayout = nullptr;
    bool isFiltering = false;

    // 以下を追加
    QToolButton* settingButton = nullptr;
    QWidget* settingPane = nullptr;
    QCheckBox* showHiddenCheckBox = nullptr;

    std::weak_ptr<EditDataManager> loadFileManager;
    QTreeWidget* treeWidget;
    bool _suspendHistory;

    GraphicsImageLoader* graphicsImageLoader = nullptr;
    QThread* graphicsLoaderThread = nullptr;
    QMap<QString, QTreeWidgetItem*> graphicsItemMap; // filePath -> item


    std::unordered_map<QString, QTreeWidgetItem*> treeTopItemList;
    std::mutex errorMutex;
    QIcon attentionIcon;
    QIcon warningIcon;


    QTreeWidgetItem* findTreeItemByFileName(const QString& fileName);
    QTreeWidgetItem* findTreeItemRecursive(QTreeWidgetItem* parent, const QString& fileName);
    void clearErrorItemsRecursive(QTreeWidgetItem* parent);

    void updateTreeVisibilityBasic(QTreeWidgetItem* parentItem);
    void updateTreeVisibilityMap(QTreeWidgetItem* parentItem);
    void updateTreeVisibilityScript(QTreeWidgetItem* parentItem);
    void updateTreeVisibilityPictures(QTreeWidgetItem* parentItem);
    bool hideTranslatedItems(QTreeWidgetItem* item);
    void rebuildErrorTree();

    bool showUntranslatedOnly = false;
    bool visibilityUpdatePending = false;


private slots:
    void onGraphicsImageLoaded(const QString& filePath, QTreeWidgetItem* item, int column, const QImage& image);
    void showContextMenu(const QPoint& position);
    void toggleItemVisibility(QTreeWidgetItem* targetItem); // hideSelectedItem をこちらに変更します
    void toggleShowHiddenFiles(bool isChecked);
    
    //検証で使用するメソッド
    void updateErrorTree();
    std::vector<std::pair<QTreeWidgetItem*, QString>> scriptErrorTargets(const ValidationErrorInfo& info, const langscore::FastCSVContainer& scriptsCsv);
};