#include "FileTree.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QThread>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QTimer>
#include "../settings.h"
#include "../csv.hpp"
#include "../invoker.h"
#include "../utility.hpp"
#include "service/GraphicsImageLoader.h"

constexpr int FILEPATH_ROLE = Qt::UserRole + 1;
constexpr int VISIBLE_ROLE = Qt::UserRole + 2;
constexpr int PREVIOUSCHECKSTATE_ROLE = Qt::UserRole + 2;
constexpr int ERROR_ITEM_ROLE = Qt::UserRole + 3;

namespace {
    QTreeWidgetItem* getTopLevelItem(QTreeWidgetItem* item) {
        while(item) {
            if(item->parent() == nullptr) { return item; }
            item = item->parent();
        }
        return nullptr;
    }

    QString getFileName(QTreeWidgetItem* item) {
        return item->data(FileTree::TreeColIndex::Name, Qt::UserRole).toString();
    }
    QString getScriptName(QTreeWidgetItem* item) {
        return item->text(FileTree::TreeColIndex::Name);
    }

    bool checkFilePathIsContainedInDirectoryPath(const QString& targetFilePath, const QString& baseDirectoryPath)
    {
        if(targetFilePath.isEmpty() == true)
        {
            return false;
        }
        if(baseDirectoryPath.isEmpty() == true)
        {
            return false;
        }

        // パスの区切り文字を '/' に統一して正規化します
        QString normalizedTargetFilePath = targetFilePath;
        normalizedTargetFilePath.replace('\\', '/');

        QString normalizedBaseDirectoryPath = baseDirectoryPath;
        normalizedBaseDirectoryPath.replace('\\', '/');

        // 基準となるディレクトリパスの末尾に '/' がある場合は削除して統一します
        if(normalizedBaseDirectoryPath.endsWith('/') == true)
        {
            normalizedBaseDirectoryPath.chop(1);
        }

        // パスが完全に一致するかどうかを確認します（Windowsのため大文字小文字を区別しません）
        if(normalizedTargetFilePath.compare(normalizedBaseDirectoryPath, Qt::CaseInsensitive) == 0)
        {
            return true;
        }

        // ディレクトリとして含まれているかを確認するため、末尾に '/' を付与した文字列を作成します
        QString directoryPrefixString = normalizedBaseDirectoryPath + "/";

        // 前方一致でパスが含まれているかを確認します
        if(normalizedTargetFilePath.startsWith(directoryPrefixString, Qt::CaseInsensitive) == true)
        {
            return true;
        }

        return false;
    }
}

FileTree::FileTree(ComponentBase* component, std::weak_ptr<CSVEditDataManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent)
    , loadFileManager(std::move(loadFileManager))
    , treeWidget(new QTreeWidget(this))
    , _suspendHistory(false)
    , graphicsImageLoader(nullptr)
    , graphicsLoaderThread(nullptr)
    , attentionIcon(QIcon(":/images/resources/image/attention.svg"))
    , warningIcon(QIcon(":/images/resources/image/warning.svg"))
{

    this->treeWidget->header()->setVisible(false);
    this->treeWidget->setColumnCount(TreeColIndex::NumColumn);

    this->treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this->treeWidget, &QTreeWidget::customContextMenuRequested, this, &FileTree::showContextMenu);

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);
    
    // 検索バーのセットアップを追加
    setupSearchBar();
    vLayout->addLayout(searchLayout);
    
    vLayout->addWidget(this->treeWidget, 1);
    this->treeWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    this->setLayout(vLayout);

    connect(this->treeWidget, &QTreeWidget::itemSelectionChanged, this, &FileTree::itemSelected);    
    // チェックボックスの状態変更を検出するための接続を追加
    connect(this->treeWidget, &QTreeWidget::itemChanged, this, &FileTree::itemChanged);

    // Setup graphics image loader thread
    graphicsImageLoader = new GraphicsImageLoader();
    graphicsLoaderThread = new QThread(this);
    graphicsImageLoader->moveToThread(graphicsLoaderThread);
    connect(graphicsImageLoader, &GraphicsImageLoader::imageLoaded,
            this, &FileTree::onGraphicsImageLoaded, Qt::QueuedConnection);
    graphicsLoaderThread->start();


}

FileTree::~FileTree()
{
    if(graphicsLoaderThread) {
        graphicsLoaderThread->quit();
        graphicsLoaderThread->wait();
        delete graphicsImageLoader;
        graphicsImageLoader = nullptr;
    }
}

void FileTree::clear()
{
    this->treeWidget->clear();
}

void FileTree::setupTree()
{
    this->treeWidget->blockSignals(true);
    this->setupBasicsTree();
    this->setupMainTree();
    this->setupScriptTree();
    this->setupGraphicsTree();
    this->updateTreeVisibility();
    this->showHiddenCheckBox->setChecked(this->setting->isShowHiddenFilesOnTree);
    this->treeWidget->blockSignals(false);
}

void FileTree::setupBasicsTree()
{
    auto files = this->setting->getBasicInfoList();

    auto basicsItem = new QTreeWidgetItem();
    basicsItem->setText(0, "Basics");
    basicsItem->setData(0, Qt::UserRole, TreeItemType::Basic);
    this->treeWidget->addTopLevelItem(basicsItem);

    // まず最初に全てのマップアイテムを作成
    for(const auto& file : files)
    {
        auto child = new QTreeWidgetItem();
        child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
        child->setData(TreeColIndex::CheckBox, PREVIOUSCHECKSTATE_ROLE, true);
        child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);

        auto fileName = file.fileName;
        child->setText(TreeColIndex::Name, fileName);
        child->setData(TreeColIndex::Name, Qt::UserRole, file.fileName);
        child->setForeground(TreeColIndex::Name, Qt::white);

        const auto& basicDataInfo = this->setting->writeObj.basicDataInfo;
        auto result = std::find_if(basicDataInfo.cbegin(), basicDataInfo.cend(), [&file](const auto& x) {
            return x.fileName.contains(file.fileName);
        });
        if(result != basicDataInfo.cend()) {
            child->setCheckState(TreeColIndex::CheckBox, result->ignore ? Qt::Unchecked : Qt::Checked);
        }

        basicsItem->addChild(child);
    }
}

void FileTree::setupMainTree() 
{
    const auto analyzeFolder = this->setting->analyzeDirectoryPath();
    auto files = this->setting->getMapInfoList();

    auto mainItem = new QTreeWidgetItem();
    mainItem->setText(0, "Main");
    mainItem->setData(0, Qt::UserRole, TreeItemType::Map);
    this->treeWidget->addTopLevelItem(mainItem);

    // マップ以外のファイル用QTreeWidgetItemを保持するマップ
    std::map<QString, QTreeWidgetItem*> fileItems;

    // マップID -> QTreeWidgetItemのマップを作成
    std::map<int, QTreeWidgetItem*> mapItems;

    // まず最初に全てのマップアイテムを作成
    for(const auto& file : files)
    {
        auto child = new QTreeWidgetItem();
        child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
        child->setData(TreeColIndex::CheckBox, PREVIOUSCHECKSTATE_ROLE, true);
        child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);

        auto fileName = file.fileName;
        if(file.mapName.isEmpty() == false) {
            fileName += " (" + file.mapName + ")";
        }
        child->setText(TreeColIndex::Name, fileName);
        child->setData(TreeColIndex::Name, Qt::UserRole, file.fileName);
        child->setData(TreeColIndex::Name, FILEPATH_ROLE, file.filePath);
        child->setForeground(TreeColIndex::Name, Qt::white);

        if(file.visible) {
            child->setData(0, VISIBLE_ROLE, true);
            child->setHidden(false);
        }
        else {
            child->setData(0, VISIBLE_ROLE, false);
            child->setHidden(true);
        }

        const auto& basicDataInfo = this->setting->writeObj.basicDataInfo;
        auto result = std::find_if(basicDataInfo.cbegin(), basicDataInfo.cend(), [&file](const auto& x) {
            return x.fileName.contains(file.fileName);
            });
        if(result != basicDataInfo.cend()) {
            child->setCheckState(TreeColIndex::CheckBox, result->ignore ? Qt::Unchecked : Qt::Checked);
        }

        // マップIDがある場合はマップアイテム辞書に追加
        if(file.mapID >= 0) {
            mapItems[file.mapID] = child;
            // この時点ではまだツリーに追加しない
        }
        else {
            // マップではないファイルは直接mainItemに追加
            mainItem->addChild(child);
            fileItems[file.fileName] = child;
        }
    }

    // マップのorderでソートするための補助構造体
    struct MapOrderInfo {
        int mapID;
        int order;
        int parentMapID;
    };

    // マップをorder順にソート
    std::vector<MapOrderInfo> mapOrderList;
    for(const auto& file : files) {
        if(file.mapID >= 0) {
            mapOrderList.push_back({file.mapID, file.order, file.parentMapID});
        }
    }

    // order順にソート
    std::sort(mapOrderList.begin(), mapOrderList.end(), [](const auto& a, const auto& b) {
        return a.order < b.order;
    });

    // 親子関係を構築（親がルート=0の場合はmainItemに、それ以外は親マップの子として追加）
    for(const auto& mapOrder : mapOrderList) {
        QTreeWidgetItem* child = mapItems[mapOrder.mapID];

        if(mapOrder.parentMapID == 0) {
            // 親がない場合はmainItemに直接追加
            mainItem->addChild(child);
        }
        else {
            // 親マップが存在する場合は親の子として追加
            auto parentIt = mapItems.find(mapOrder.parentMapID);
            if(parentIt != mapItems.end()) {
                parentIt->second->addChild(child);
            }
            else {
                // 親が見つからない場合はmainItemに追加
                mainItem->addChild(child);
            }
        }
    }
}

void FileTree::setupScriptTree() 
{

    auto scriptItem = new QTreeWidgetItem();
    scriptItem->setText(0, "Script");
    scriptItem->setData(0, Qt::UserRole, TreeItemType::Script);
    this->treeWidget->addTopLevelItem(scriptItem);

    auto scriptFiles = this->setting->getScriptFileList();
    auto scriptExtention = settings::scriptExt(setting->projectType);
    for(const auto& l : scriptFiles)
    {
        const auto& scriptFileName = l.fileName;
        const auto& scriptName = l.scriptName;
        if(scriptName == "_NONAME_") {
            continue;
        }
        if(settings::isLangscoreScript(scriptName)) {
            continue;
        }

        //ファイルの中身に有効な文字が含まれているかをチェック
        bool isEmptyFile = false;
        QFile scriptFile(l.filePath + scriptExtention);
        if(scriptFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            auto content = scriptFile.readAll();
            isEmptyFile = content.isEmpty() || content.trimmed().isEmpty();
            scriptFile.close();
        }

        auto child = new QTreeWidgetItem();
        if(isEmptyFile == false) {
            child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
            child->setData(TreeColIndex::CheckBox, PREVIOUSCHECKSTATE_ROLE, true);
            //チェックを外すとこのスクリプトを翻訳から除外します。
            child->setToolTip(TreeColIndex::CheckBox, tr("Unchecking the box excludes this script from translation."));
            child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
        }
        else {
            child->setFlags(Qt::ItemIsSelectable);
        }
        child->setText(TreeColIndex::Name, scriptName);
        child->setData(TreeColIndex::Name, Qt::UserRole, langscore::withoutExtension(scriptFileName));
        child->setData(TreeColIndex::Name, FILEPATH_ROLE, l.filePath);

        if(l.visible) {
            child->setData(0, VISIBLE_ROLE, true);
            child->setHidden(false);
        }
        else {
            child->setData(0, VISIBLE_ROLE, false);
            child->setHidden(true);
        }

        const auto& scriptList = this->setting->writeObj.scriptInfo;
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&scriptFileName](const auto& script) {
            return script.fileName.contains(scriptFileName);
        });
        if(result != scriptList.cend())
        {
            if(result->ignore) {
                child->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked);
            }
            else if(0 == std::ranges::count(result->lines, true, &langscore::TextPosition::ignore)) {
                child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
            }
            else {
                child->setCheckState(TreeColIndex::CheckBox, Qt::PartiallyChecked);
            }
        }

        scriptItem->addChild(child);
    }
}

void FileTree::setupGraphicsTree() 
{
    auto graphicsRootItem = new QTreeWidgetItem();
    graphicsRootItem->setText(0, "Graphics");
    graphicsRootItem->setData(0, Qt::UserRole, TreeItemType::Pictures);
    this->treeWidget->addTopLevelItem(graphicsRootItem);

    auto graphList = this->setting->getGraphicsFileList();
    // Map: full path -> QTreeWidgetItem*
    QMap<QString, QTreeWidgetItem*> folderItemMap;
    folderItemMap[""] = graphicsRootItem; // root 

    graphicsItemMap.clear();

    auto treeItemHeight = this->treeWidget->sizeHintForRow(0);

    for(const auto& graphInfo : graphList)
    {
        //ファイル前提で組まれた処理なので、フォルダなら無視
        if(graphInfo.isFolder) { continue; }

        // Build directory hierarchy
        QString relPath = graphInfo.folder;
        QStringList parts = relPath.split(QDir::separator(), Qt::SkipEmptyParts);
        QString currentPath;
        QTreeWidgetItem* parentItem = graphicsRootItem;

        for(const QString& part : parts) {
            if(false == currentPath.isEmpty()) {
                currentPath += QDir::separator();
            }
            currentPath += part;

            if(!folderItemMap.contains(currentPath)) {
                auto* dirItem = new QTreeWidgetItem();
                dirItem->setText(TreeColIndex::Name, part);
                dirItem->setData(0, Qt::UserRole, TreeItemType::Pictures);
                QFileInfo url{graphInfo.filePath};
                auto path = url.filesystemFilePath();
                dirItem->setData(TreeColIndex::Name, FILEPATH_ROLE, QString::fromStdString(path.parent_path().string()));
                parentItem->addChild(dirItem);
                folderItemMap[currentPath] = dirItem;
            }
            parentItem = folderItemMap[currentPath];
        }

        // Add image item to the deepest directory node
        auto pictureFileName = graphInfo.fileName;
        auto* child = new QTreeWidgetItem();
        child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
        child->setData(TreeColIndex::CheckBox, PREVIOUSCHECKSTATE_ROLE, true);
        child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
        child->setText(TreeColIndex::Name, pictureFileName + "." + graphInfo.extension);
        child->setData(TreeColIndex::Name, FILEPATH_ROLE, graphInfo.filePath);
        child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
        child->setForeground(0, Qt::white);

        if(graphInfo.visible) {
            child->setData(0, VISIBLE_ROLE, true);
            child->setHidden(false);
        }
        else {
            child->setData(0, VISIBLE_ROLE, false);
            child->setHidden(true);
        }

        auto& pixtureList = this->setting->writeObj.ignorePicturePath;
        auto result = std::find_if(pixtureList.cbegin(), pixtureList.cend(), [&pictureFileName](const auto& pic) {
            return QFileInfo(pic).completeBaseName() == pictureFileName;
        });
        if(result != pixtureList.end()) {
            child->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked);
        }
        else {
            child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
        }

        parentItem->addChild(child);

        // Queue image loading in background
        graphicsItemMap[graphInfo.filePath] = child;
        graphicsImageLoader->requestImage(graphInfo.filePath, child, TreeColIndex::Name, QSize{treeItemHeight, treeItemHeight});
    }

    // Set check state for folders
    for (auto it = folderItemMap.begin(); it != folderItemMap.end(); ++it) {
        QTreeWidgetItem* folderRoot = it.value();
        if(folderRoot == graphicsRootItem) continue;
        if(0 < folderRoot->childCount())
        {
            folderRoot->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
            folderRoot->setData(TreeColIndex::CheckBox, PREVIOUSCHECKSTATE_ROLE, true);

            int ignorePictures = 0;
            for(int i = 0; i < folderRoot->childCount(); ++i) {
                if(folderRoot->child(i)->checkState(TreeColIndex::CheckBox) == Qt::Unchecked)
                    ++ignorePictures;
            }
            if(ignorePictures == 0) { folderRoot->setCheckState(TreeColIndex::CheckBox, Qt::Checked); }
            else if(ignorePictures < folderRoot->childCount()) { folderRoot->setCheckState(TreeColIndex::CheckBox, Qt::PartiallyChecked); }
            else { folderRoot->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked); }
        }
    }
}

void FileTree::itemSelected()
{
    auto selectedItems = this->treeWidget->selectedItems();
    if(selectedItems.empty()) { return; }
    auto item = selectedItems[0];
    auto topLevelItem = getTopLevelItem(item);
    if(topLevelItem == nullptr) { return; }

    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
    if(treeType == TreeItemType::Map || treeType == TreeItemType::Basic) {
        auto fileName = item->data(TreeColIndex::Name, Qt::UserRole).toString();
        emit setTabIndex(1);
        emit showMainFile(item->text(TreeColIndex::Name), fileName);
    }
    else if(treeType == TreeItemType::Script)
    {
        auto changedItemScriptFileName = ::getFileName(item);
        emit setTabIndex(2);

        auto scriptExt = langscore::GetScriptExtension(this->setting->projectType);
        auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + changedItemScriptFileName + scriptExt;
        emit showScriptFile(scriptFilePath);

        // テーブル側の行を探してスクロール・選択
        emit scriptTableScrollToRow(changedItemScriptFileName);
        emit scriptTableSelectRow(changedItemScriptFileName);
    }
    else if(treeType == TreeItemType::Pictures)
    {
        if(item == topLevelItem) { return; }
        if(item->parent() == topLevelItem) { return; }

        auto path = ::getFileName(item);
        if(path.isEmpty()) { return; }
        emit showGraphicsImage(path);
        emit setTabIndex(3);
    }
}

void FileTree::itemChanged(QTreeWidgetItem* _item, int column)
{
    if(_item == nullptr) {
        return;
    }

    Qt::CheckState currentCheckState = _item->checkState(column);
    QVariant previousCheckStateVariant = _item->data(column, PREVIOUSCHECKSTATE_ROLE);

    if(previousCheckStateVariant.isValid() == true) {
        Qt::CheckState previousCheckState = static_cast<Qt::CheckState>(previousCheckStateVariant.toInt());

        if(previousCheckState == currentCheckState) {
            return;
        }
    }

    _item->setData(column, PREVIOUSCHECKSTATE_ROLE, static_cast<int>(currentCheckState));

    auto topLevelItem = ::getTopLevelItem(_item);
    if(topLevelItem == nullptr) { return; }
    if(column != 0) { return; }

    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);

    auto selectedItems = this->treeWidget->selectedItems();
    if(selectedItems.contains(_item) == false) {
        selectedItems.clear();
        selectedItems.emplace_back(_item);
    }

    const auto NotifyTreeUndoCommand = [this](std::vector<QUndoCommand*> result, QString groupName) {

        if(result.empty()) { return; }
        if(_suspendHistory) {
            this->discardCommand(std::move(result));
            return;
        }
        const QSignalBlocker blocker(this->treeWidget);
        if(result.size() == 1) {
            this->history->push(result[0]);
        }
        else
        {
            this->history->beginMacro(groupName);
            for(auto* command : result) {
                this->history->push(command);
            }
            this->history->endMacro();
        }
    };

    auto newCheckVal = _item->checkState(0);
    if(treeType == TreeItemType::Basic)
    {
        auto fileName = ::getFileName(_item);
        const auto& basicData = this->setting->fetchBasicDataInfo(fileName);
        auto treeCheckState = basicData.ignore ? Qt::Unchecked : Qt::Checked;
        std::vector<QUndoCommand*> result;
        for(auto item : selectedItems) {
            if(newCheckVal == treeCheckState) { continue; }
            result.emplace_back(new FileTreeUndo(this, item, newCheckVal, treeCheckState));
        }

        NotifyTreeUndoCommand(std::move(result), tr("Basics Tree Change Enable State"));
    }
    else if(treeType == TreeItemType::Map)
    {
        auto fileName = ::getFileName(_item);
        const auto& info = this->setting->fetchMapInfo(fileName);
        auto treeCheckState = info.ignore ? Qt::Unchecked : Qt::Checked;
        std::vector<QUndoCommand*> result;
        for(auto item : selectedItems) {
            if(newCheckVal == treeCheckState) { continue; }
            result.emplace_back(new FileTreeUndo(this, item, newCheckVal, treeCheckState));
        }

        NotifyTreeUndoCommand(std::move(result), tr("Map Tree Change Enable State"));
    }
    else if(treeType == TreeItemType::Script)
    {
        auto fileName = ::getFileName(_item);
        const auto& scriptList = this->setting->fetchScriptInfo(fileName);
        //変更前(現在)のチェック状態を取得
        auto treeCheckState = Qt::Checked;
        if(scriptList.ignore) {
            treeCheckState = Qt::Unchecked;
            newCheckVal = Qt::Checked;
        }
        else if(scriptList.lines.empty() == false) {
            treeCheckState = Qt::PartiallyChecked;
            newCheckVal = Qt::Unchecked;
        }

        if(newCheckVal == Qt::Checked) {
            //チェックを付ける場合は、テーブルの選択状態によってツリーのチェック状態を変更。
            auto scriptName = ::getScriptName(_item);
            //newCheckVal = this->loadFileManager.lock()->getTreeCheckStateBasedOnTable(scriptName);
        }
        std::vector<QUndoCommand*> result;
        for(auto item : selectedItems) {
            if(newCheckVal == treeCheckState) { continue; }
            result.emplace_back(new FileTreeUndo(this, item, newCheckVal, treeCheckState));
        }

        NotifyTreeUndoCommand(std::move(result), tr("Script Tree Change Enable State"));
    }
    else if(treeType == TreeItemType::Pictures)
    {
        std::vector<QTreeWidgetItem*> checkItems;
        for(auto item : selectedItems)
        {
            //画像アイテムの親が選択されていたら、そちらを優先する。
            if(item->parent() == topLevelItem) {
                checkItems.emplace_back(item);
            }
        }

        for(auto item : selectedItems)
        {
            if(std::find(checkItems.cbegin(), checkItems.cend(), item) != checkItems.cend()) {
                continue;
            }
            if(std::find(checkItems.cbegin(), checkItems.cend(), item->parent()) != checkItems.cend()) {
                continue;
            }
            checkItems.emplace_back(item);
        }

        this->treeWidget->blockSignals(true);
        std::vector<QUndoCommand*> result;
        for(auto* item : checkItems)
        {
            result.emplace_back(new FileTreeUndo(this, item, newCheckVal, item->checkState(0)));
            //フォルダーのチェックを変更
            if(item->parent() == topLevelItem)
            {
                //親が選択されていた場合、親優先でチェック状態を変更する。
                //この時、子のチェック状態も合わせて変更する。
                auto numChilds = item->childCount();
                for(int i = 0; i < numChilds; ++i) {
                    auto childItem = item->child(i);
                    result.emplace_back(new FileTreeUndo(this, childItem, newCheckVal, childItem->checkState(0)));
                }
            }
            else
            {
                //同階層のオブジェクトチェック
                auto folderItem = item->parent();
                const auto numChilds = folderItem->childCount();
                int numChecked = 0;
                for(int i = 0; i < numChilds; ++i) {
                    auto childItem = folderItem->child(i);
                    if(childItem->checkState(0) == Qt::Checked) {
                        numChecked++;
                    }
                }
                auto parentFolderCheckState = Qt::Checked;
                if(numChecked == 0) {
                    parentFolderCheckState = Qt::Unchecked;
                }
                else if(numChecked < numChilds) {
                    parentFolderCheckState = Qt::PartiallyChecked;
                }
                folderItem->setCheckState(0, parentFolderCheckState);
            }
        }
        this->treeWidget->blockSignals(false);

        NotifyTreeUndoCommand(std::move(result), tr("Graphics Tree Change Enable State"));

    }

}


void FileTree::setTreeItemCheck(QTreeWidgetItem* item, Qt::CheckState check)
{
    const bool ignore = check == Qt::Unchecked;
    auto topLevelItem = getTopLevelItem(item);
    if(topLevelItem == nullptr) { return; }
    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
    item->setCheckState(0, check);

    // ---------------------------------------

    if(treeType == TreeItemType::Basic) {
        writeToBasicDataListSetting(item, ignore);
        this->treeWidget->update();
        return;
    }
    else if(treeType == TreeItemType::Map) {
        writeToMapDataListSetting(item, ignore);
        this->treeWidget->update();
        return;
    }
    else if(treeType == TreeItemType::Pictures) {
        writeToPictureListSetting(item, ignore);
        this->treeWidget->update();
        return;
    }

    //スクリプトのアイテムのチェックを変更
    writeToScriptListSetting(item, ignore);

    if(treeType == TreeItemType::Script) {
        auto scriptName = ::getScriptName(item);
        emit notifyScriptTreeItemCheckChanged(scriptName, check);
    }
    // -----------------------------------------
}

void FileTree::writeToBasicDataListSetting(QTreeWidgetItem* item, bool ignore)
{
    auto fileName = ::getFileName(item);
    auto& info = this->setting->fetchBasicDataInfo(fileName);
    info.ignore = ignore;
}

void FileTree::writeToMapDataListSetting(QTreeWidgetItem* item, bool ignore)
{
    auto fileName = ::getFileName(item);
    auto& info = this->setting->fetchMapInfo(fileName);
    info.ignore = ignore;
}

void FileTree::writeToScriptListSetting(QTreeWidgetItem* item, bool ignore)
{
    auto fileName = ::getFileName(item);
    auto& info = this->setting->fetchScriptInfo(fileName);
    info.ignore = ignore;
}

void FileTree::writeToPictureListSetting(QTreeWidgetItem* item, bool ignore)
{
    auto filePath = ::getFileName(item);
    if(filePath.isEmpty()) { return; }
    QDir graphicsDir(this->setting->tempGraphicsFileDirectoryPath());
    filePath = graphicsDir.relativeFilePath(filePath);
    auto& picturePathList = this->setting->writeObj.ignorePicturePath;
    auto result = std::find_if(picturePathList.cbegin(), picturePathList.cend(), [&filePath](const auto& path) {
        return path == filePath;
        });
    if(result != picturePathList.end()) {
        if(ignore == false) {
            picturePathList.erase(result);
        }
    }
    else if(ignore) {
        picturePathList.emplace_back(filePath);
    }
}

void FileTree::onChangeScriptTableItemCheck(QString scriptName, Qt::CheckState check)
{
    auto scriptNameItem = this->fetchScriptTreeSameFileItem(scriptName);
    if(scriptNameItem == nullptr) { return; }

    auto scriptFileName = ::getFileName(scriptNameItem);
    const auto& scriptList = this->setting->writeObj.scriptInfo;
    auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&scriptFileName](const auto& script) {
        return script.fileName.contains(scriptFileName);
    });

    this->treeWidget->blockSignals(true);
    if(result != scriptList.cend())
    {
        auto ignoredCount = std::ranges::count(result->lines, true, &langscore::TextPosition::ignore);
        if(result->ignore || result->lines.size() == ignoredCount) {
            scriptNameItem->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked);
        }
        else if(0 == ignoredCount) {
            scriptNameItem->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
        }
        else {
            scriptNameItem->setCheckState(TreeColIndex::CheckBox, Qt::PartiallyChecked);
        }
    }
    this->treeWidget->blockSignals(false);
}


QTreeWidgetItem* FileTree::fetchScriptTreeSameFileItem(QString scriptName)
{
    const auto numItems = this->treeWidget->topLevelItemCount();
    for(int i = 0; i < numItems; ++i)
    {
        auto parentItem = this->treeWidget->topLevelItem(i);
        const auto treeType = parentItem->data(TreeColIndex::CheckBox, Qt::UserRole);
        if(treeType != TreeItemType::Script) { continue; }

        auto numChilds = parentItem->childCount();
        for(int c = 0; c < numChilds; ++c)
        {
            auto childItem = parentItem->child(c);
            if(scriptName != ::getScriptName(childItem)) { continue; }
            return childItem;
        }
    }
    return nullptr;
}

void FileTree::updateTreeTextColor()
{
    QColor topLevelItemBGColor = this->getColorTheme().getTopLevelItemBGColor();
    QColor graphicFolderBGColor = this->getColorTheme().getGraphicFolderBGColor();
    QColor itemTextColor = this->getColorTheme().getItemTextColor();

    this->treeWidget->blockSignals(true);
    auto numToplevelItem = this->treeWidget->topLevelItemCount();
    for(int i = 0; i < numToplevelItem; ++i)
    {
        auto item = this->treeWidget->topLevelItem(i);
        item->setBackground(0, topLevelItemBGColor);
        item->setBackground(1, topLevelItemBGColor);
        item->setForeground(0, itemTextColor);
        item->setForeground(1, itemTextColor);
    }
    this->treeWidget->blockSignals(false);
    this->update();
}

void FileTree::onGraphicsImageLoaded(const QString& filePath, QTreeWidgetItem* item, int column, const QImage& image)
{
    if(!item) { return; }
    if(image.isNull() == false) {
        this->_suspendHistory = true;
        item->setIcon(column, QIcon(QPixmap::fromImage(image)));
        this->_suspendHistory = false;
    }
}

// ファイルの末尾に以下のメソッドを追加

void FileTree::setupSearchBar()
{
    searchLayout = new QHBoxLayout();
    searchLayout->setContentsMargins(0, 0, 0, 0);
    searchLayout->setSpacing(0);

    searchLineEdit = new QLineEdit(this);
    searchLineEdit->setPlaceholderText(tr("Search..."));
    searchLineEdit->setClearButtonEnabled(true);

    // ポップアップ領域のセットアップ
    settingPane = new QWidget(this);
    settingPane->setWindowFlags(Qt::Popup);
    settingPane->setVisible(false);

    auto popupLayout = new QVBoxLayout(this);
    settingPane->setLayout(popupLayout);

    // ポップアップの中にチェックボックスを配置
    showHiddenCheckBox = new QCheckBox(tr("Show Hidden Items"), settingPane);
    showHiddenCheckBox->setChecked(this->setting->isShowHiddenFilesOnTree);
    connect(showHiddenCheckBox, &QCheckBox::toggled, this, &FileTree::toggleShowHiddenFiles);

    popupLayout->addWidget(showHiddenCheckBox);

    // 設定ボタンのセットアップ
    settingButton = new QToolButton(this);
    settingButton->setText(tr("Settings"));
    connect(settingButton, &QToolButton::clicked, this, [this](bool) {
        this->settingPane->setVisible(true);
        this->settingPane->move(QCursor::pos());
    });

    // レイアウトには検索バーと設定ボタンを追加する
    searchLayout->addWidget(searchLineEdit);
    searchLayout->addWidget(settingButton);

    connect(searchLineEdit, &QLineEdit::textChanged, this, &FileTree::filterTree);
}

void FileTree::filterTree(const QString& searchText)
{
    if(searchText.isEmpty()) {
        resetTreeFilter();
        return;
    }

    isFiltering = true;

    // トップレベルアイテムを処理
    for(int i = 0; i < treeWidget->topLevelItemCount(); i++) {
        QTreeWidgetItem* topItem = treeWidget->topLevelItem(i);
        topItem->setHidden(false); // トップレベルは常に表示

        // 子アイテムを再帰的に処理
        bool hasVisibleChildren = false;
        for(int j = 0; j < topItem->childCount(); j++) {
            QTreeWidgetItem* childItem = topItem->child(j);
            bool matched = filterTreeItem(childItem, searchText);
            if(matched) {
                hasVisibleChildren = true;
            }
        }

        // マッチする子アイテムがある場合は展開
        if(hasVisibleChildren) {
            topItem->setExpanded(true);
            expandItemsWithChildren(topItem);
        }
    }
}

bool FileTree::filterTreeItem(QTreeWidgetItem* item, const QString& searchText, bool parentMatched)
{
    if(!item) { return false; }

    bool matched = item->text(TreeColIndex::Name).contains(searchText, Qt::CaseInsensitive);

    // 子アイテムがある場合は再帰的に処理
    bool hasMatchingChild = false;
    for(int i = 0; i < item->childCount(); i++) {
        if(filterTreeItem(item->child(i), searchText, matched || parentMatched)) {
            hasMatchingChild = true;
        }
    }

    // このアイテムかその子がマッチするか、親がマッチする場合は表示
    bool shouldShow = matched || hasMatchingChild || parentMatched;

    if(shouldShow) {
        auto visible = item->data(0, VISIBLE_ROLE);
        if(visible.isValid() || visible.toBool() == false) {
            shouldShow = false;
        }
    }

    item->setHidden(!shouldShow);

    // マッチした場合は強調表示
    if(matched && !parentMatched) {
        item->setForeground(TreeColIndex::Name, Qt::yellow);
    }
    else {
        item->setForeground(TreeColIndex::Name, getColorTheme().getItemTextColor());
    }

    return shouldShow;
}

void FileTree::resetTreeFilter()
{
    if(isFiltering == false) { return; }

    isFiltering = false;

    // すべてのアイテムを表示に戻し、元の色に戻す
    for(int i = 0; i < treeWidget->topLevelItemCount(); i++) {
        QTreeWidgetItem* topItem = treeWidget->topLevelItem(i);
        auto visible = topItem->data(0, VISIBLE_ROLE);
        if(visible.isValid() == false || visible.toBool()) {
            topItem->setHidden(false);
        }
        else {
            topItem->setHidden(true);
        }

        // 子アイテムを再帰的に処理
        for(int j = 0; j < topItem->childCount(); j++) {
            resetItemVisibility(topItem->child(j));
        }

        // トップレベルアイテムを元の状態に戻す
        topItem->setExpanded(false);
    }

    // ツリーの表示を更新
    updateTreeTextColor();
}

void FileTree::expandItemsWithChildren(QTreeWidgetItem* item)
{
    for(int i = 0; i < item->childCount(); i++) {
        QTreeWidgetItem* childItem = item->child(i);
        if(!childItem->isHidden() && childItem->childCount() > 0) {
            childItem->setExpanded(true);
            expandItemsWithChildren(childItem);
        }
    }
}

void FileTree::resetItemVisibility(QTreeWidgetItem* item)
{
    if(!item) { return; }

    auto visible = item->data(0, VISIBLE_ROLE);
    if(visible.isValid() == false || visible.toBool()) {
        item->setHidden(false);
    }
    else {
        item->setHidden(true);
    }

    item->setForeground(TreeColIndex::Name, getColorTheme().getItemTextColor());

    // 子アイテムも再帰的に処理
    for(int i = 0; i < item->childCount(); i++) {
        resetItemVisibility(item->child(i));
    }
}

void FileTree::receive(DispatchType type, const QVariantList& args)
{
    if(type == DispatchType::NotifyFinishValidateCSV)
    {
        this->updateErrorTree();
    }
}

void FileTree::FileTreeUndo::setValue(ValueType value) {
    this->parent->setTreeItemCheck(this->target, value);
}

void FileTree::FileTreeUndo::undo() {
    this->setValue(oldValue);
    this->setText(tr("Change Tree State : %1").arg(this->target->text(1)));
}

void FileTree::FileTreeUndo::redo() {
    this->setValue(newValue);
    this->setText(tr("Change Tree State : %1").arg(this->target->text(1)));
}

void FileTree::showContextMenu(const QPoint& position)
{
    QTreeWidgetItem* targetItem = this->treeWidget->itemAt(position);
    if(targetItem == nullptr) {
        return;
    }

    auto topLevelItem = ::getTopLevelItem(targetItem);
    if(targetItem == topLevelItem) {
        return;
    }

    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
    auto fileName = ::getFileName(targetItem);
    bool isVisible = true;

    auto visible = targetItem->data(0, VISIBLE_ROLE);
    if(visible.isValid()) {
        isVisible = visible.toBool();
    }

    QMenu contextMenu(this);
    QString actionText;

    if(isVisible == true) {
        actionText = tr("Hide Item");
    }
    else {
        actionText = tr("Show Item");
    }

    QAction* toggleAction = new QAction(actionText, this);
    connect(toggleAction, &QAction::triggered, this, [this, targetItem]() {
        this->toggleItemVisibility(targetItem);
    });
    contextMenu.addAction(toggleAction);

    contextMenu.exec(this->treeWidget->mapToGlobal(position));
}

void FileTree::toggleItemVisibility(QTreeWidgetItem* targetItem)
{
    if(targetItem == nullptr) {
        return;
    }

    auto topLevelItem = ::getTopLevelItem(targetItem);
    if(topLevelItem == nullptr) {
        return;
    }

    QSignalBlocker blocker(this->treeWidget);

    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
    auto fileName = ::getFileName(targetItem);

    if(treeType == TreeItemType::Basic) {
        auto& information = this->setting->fetchBasicDataInfo(fileName);
        if(information.visible == true) {
            information.visible = false;
        }
        else {
            information.visible = true;
        }
        targetItem->setData(0, VISIBLE_ROLE, information.visible);
        if(this->setting->isShowHiddenFilesOnTree) {
            targetItem->setHidden(false);
        }
        else {
            targetItem->setHidden(information.visible == false);
        }
    }
    else if(treeType == TreeItemType::Map) {
        auto& information = this->setting->fetchMapInfo(fileName);
        if(information.visible == true) {
            information.visible = false;
        }
        else {
            information.visible = true;
        }
        targetItem->setData(0, VISIBLE_ROLE, information.visible);
        if(this->setting->isShowHiddenFilesOnTree) {
            targetItem->setHidden(false);
        }
        else {
            targetItem->setHidden(information.visible == false);
        }
    }
    else if(treeType == TreeItemType::Script) {
        auto& information = this->setting->fetchScriptInfo(fileName);
        if(information.visible == true) {
            information.visible = false;
        }
        else {
            information.visible = true;
        }
        targetItem->setData(0, VISIBLE_ROLE, information.visible);
        if(this->setting->isShowHiddenFilesOnTree) {
            targetItem->setHidden(false);
        }
        else {
            targetItem->setHidden(information.visible == false);
        }
    }
    else if(treeType == TreeItemType::Pictures) {
        auto filePath = targetItem->data(TreeColIndex::Name, FILEPATH_ROLE).toString();
        auto& graphList = this->setting->writeObj.graphDataInfo;
        for(auto& graphInformation : graphList) {
            if(checkFilePathIsContainedInDirectoryPath(graphInformation.filePath, filePath)) {
                if(graphInformation.visible == true) {
                    graphInformation.visible = false;
                }
                else {
                    graphInformation.visible = true;
                }
                targetItem->setData(0, VISIBLE_ROLE, graphInformation.visible);
                if(this->setting->isShowHiddenFilesOnTree) {
                    targetItem->setHidden(false);
                }
                else {
                    targetItem->setHidden(graphInformation.visible == false);
                }
                break;
            }
        }
    }

    this->update();
}

void FileTree::toggleShowHiddenFiles(bool isChecked)
{
    this->setting->isShowHiddenFilesOnTree = isChecked;
    this->updateTreeVisibility();
}

void FileTree::updateTreeVisibility()
{
    QSignalBlocker blocker(this->treeWidget);

    std::function<void(TreeItemType, QTreeWidgetItem*)> updateChildrenVisibility;
    updateChildrenVisibility = [this, &updateChildrenVisibility](TreeItemType treeType, QTreeWidgetItem* parentItem)
    {
        for(int j = 0; j < parentItem->childCount(); j++) {
            QTreeWidgetItem* childItem = parentItem->child(j);
            bool isVisible = true;

            auto fileName = ::getFileName(childItem);

            if(treeType == TreeItemType::Basic) {
                const auto& information = this->setting->fetchBasicDataInfo(fileName);
                isVisible = information.visible;
            }
            else if(treeType == TreeItemType::Map) {
                const auto& information = this->setting->fetchMapInfo(fileName);
                isVisible = information.visible;
            }
            else if(treeType == TreeItemType::Script) {
                const auto& information = this->setting->fetchScriptInfo(fileName);
                isVisible = information.visible;
            }
            else if(treeType == TreeItemType::Pictures) {
                auto filePath = childItem->data(TreeColIndex::Name, FILEPATH_ROLE).toString();
                const auto& graphList = this->setting->writeObj.graphDataInfo;
                for(const auto& graphInformation : graphList) {
                    if(checkFilePathIsContainedInDirectoryPath(graphInformation.filePath, filePath)) {
                        isVisible = graphInformation.visible;
                        break;
                    }
                }
            }

            //VISIBLE_ROLEは設定ファイル側と同値にするため、ここで変更しないこと。
            if(isVisible == false && this->setting->isShowHiddenFilesOnTree == false) {
                childItem->setHidden(true);
            }
            else {
                childItem->setHidden(false);
            }

            if(0 < childItem->childCount()) {
                updateChildrenVisibility(treeType, childItem);
            }
        }
    };

    for(int i = 0; i < this->treeWidget->topLevelItemCount(); i++) 
    {
        QTreeWidgetItem* topLevelItem = this->treeWidget->topLevelItem(i);
        const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
        updateChildrenVisibility(static_cast<TreeItemType>(treeType.toInt()), topLevelItem);
    }

    this->update();
}



void FileTree::clearErrors()
{
    std::lock_guard<std::mutex> locker(this->errorMutex);
    this->runtimeData->errors.clear();
    this->runtimeData->updateList.clear();
    this->errorInfoIndex = 0;

    // 既存のツリーアイテムからエラー表示（子アイテムとアイコン）を削除
    const int topLevelCount = this->treeWidget->topLevelItemCount();
    for(int i = 0; i < topLevelCount; ++i)
    {
        QTreeWidgetItem* topItem = this->treeWidget->topLevelItem(i);
        this->clearErrorItemsRecursive(topItem);
    }
}

void FileTree::clearErrorItemsRecursive(QTreeWidgetItem* parent)
{
    // 子アイテムを削除するため、後ろからループ処理を行う
    for(int i = parent->childCount() - 1; i >= 0; --i)
    {
        QTreeWidgetItem* child = parent->child(i);

        // エラーアイテムとして追加されたものか判定する
        if(child->data(TreeColIndex::Name, ERROR_ITEM_ROLE).toBool() == true)
        {
            parent->removeChild(child);
            delete child;
        }
        else
        {
            // 通常のファイルアイテムならアイコンを消去し、さらにその子を再帰的にチェック
            child->setIcon(TreeColIndex::Name, QIcon());
            if(child->childCount() > 0)
            {
                this->clearErrorItemsRecursive(child);
            }
        }
    }
}

QTreeWidgetItem* FileTree::findTreeItemByFileName(const QString& fileName)
{
    const int topLevelCount = this->treeWidget->topLevelItemCount();
    for(int i = 0; i < topLevelCount; ++i)
    {
        QTreeWidgetItem* topItem = this->treeWidget->topLevelItem(i);
        QTreeWidgetItem* foundItem = this->findTreeItemRecursive(topItem, fileName);
        if(foundItem != nullptr)
        {
            return foundItem;
        }
    }
    return nullptr;
}

QTreeWidgetItem* FileTree::findTreeItemRecursive(QTreeWidgetItem* parent, const QString& fileName)
{
    const int childCount = parent->childCount();
    for(int i = 0; i < childCount; ++i)
    {
        QTreeWidgetItem* child = parent->child(i);

        QString itemFileName = child->data(TreeColIndex::Name, Qt::UserRole).toString();

        // ファイル名が完全に一致するか判定する
        if(itemFileName == fileName)
        {
            return child;
        }

        if(child->childCount() > 0)
        {
            QTreeWidgetItem* foundItem = this->findTreeItemRecursive(child, fileName);
            if(foundItem != nullptr)
            {
                return foundItem;
            }
        }
    }
    return nullptr;
}

void FileTree::addErrorText(QString text)
{
    if(text.isEmpty() == true)
    {
        return;
    }

    std::vector<ValidationErrorInfo> parsedInfos = this->processJsonBuffer(text);

    this->errorMutex.lock();
    for(auto&& info : parsedInfos)
    {
        QFileInfo fileInfo(info.filePath);
        QString fileName = fileInfo.baseName();

        this->runtimeData->errors[fileName].emplace_back(std::move(info));
        this->runtimeData->updateList[fileName] = true;
    }
    this->errorMutex.unlock();

    this->update();
}

void FileTree::updateErrorTree()
{
    bool updated = false;
    for(auto& updateInfo : this->runtimeData->updateList)
    {
        if(updateInfo.second == false)
        {
            continue;
        }
        updated = true;

        std::lock_guard<std::mutex> locker(this->errorMutex);
        auto& infoList = this->runtimeData->errors[updateInfo.first];

        QTreeWidgetItem* targetItem = this->findTreeItemByFileName(updateInfo.first);
        if(targetItem == nullptr)
        {
            updateInfo.second = false;
            continue;
        }

        for(auto& info : infoList)
        {
            if(info.shown == true)
            {
                continue;
            }

            // アイコンの設定（エラー優先で、警告は次点とする）
            auto errorIter = std::find_if(infoList.cbegin(), infoList.cend(), [](const auto& infoItem) {
                return infoItem.type == ValidationErrorInfo::Error;
            });

            if(errorIter != infoList.cend())
            {
                targetItem->setIcon(TreeColIndex::Name, this->attentionIcon);
            }
            else
            {
                auto warnIter = std::find_if(infoList.cbegin(), infoList.cend(), [](const auto& infoItem) {
                    return infoItem.type == ValidationErrorInfo::Warning;
                });

                if(warnIter != infoList.cend())
                {
                    targetItem->setIcon(TreeColIndex::Name, this->warningIcon);
                }
            }

            // エラー表示用の子アイテムを構築する
            QTreeWidgetItem* child = new QTreeWidgetItem();
            QString text;

            if(info.type == ValidationErrorInfo::Warning)
            {
                child->setIcon(TreeColIndex::Name, this->warningIcon);
                child->setForeground(TreeColIndex::Name, QColor(240, 227, 0));
            }
            else if(info.type == ValidationErrorInfo::Error)
            {
                child->setIcon(TreeColIndex::Name, this->attentionIcon);
                child->setForeground(TreeColIndex::Name, QColor(236, 11, 0));
            }

            switch(info.summary)
            {
            case ValidationErrorInfo::EmptyCol:
                text += tr(" Empty Column") + "[" + info.language + "]";
                break;
            case ValidationErrorInfo::NotFoundEsc:
                text += tr(" Not Found Esc") + "[" + info.language + "] (" + info.detail + ")";
                break;
            case ValidationErrorInfo::UnclosedEsc:
                text += tr(" Unclosed Esc") + "[" + info.language + "] (" + info.detail + ")";
                break;
            case ValidationErrorInfo::IncludeCR:
                text += tr(" Include \"\r\n\"");
                break;
            case ValidationErrorInfo::NotEQLang:
                text += tr(" The specified language does not match the language in the CSV");
                break;
            case ValidationErrorInfo::PartiallyClipped:
                text += tr(" Part of this text is cut off.");
                break;
            case ValidationErrorInfo::FullyClipped:
                text += tr(" This text is completely cut off.");
                break;
            case ValidationErrorInfo::OverTextCount:
                text += tr(" The specified number of characters has been exceeded. (num %1)").arg(info.width);
                break;
            case ValidationErrorInfo::InvalidCSV:
                if(info.detail.isEmpty() == true)
                {
                    text += tr(" Invalid CSV, This may be due to the description around the %1 line.").arg(info.row);
                }
                else
                {
                    text += tr(" Invalid CSV. The description around %2 in the %1 row may be cause.").arg(info.row).arg(info.detail);
                }
                break;
            default:
                break;
            }

            child->setText(TreeColIndex::Name, tr("Line") + QString::number(info.row) + " : " + text);
            child->setData(TreeColIndex::Name, Qt::UserRole, info.id);
            child->setData(TreeColIndex::Name, ERROR_ITEM_ROLE, true); // エラーアイテムとしての識別用フラグ

            targetItem->addChild(child);
            targetItem->setExpanded(true); // エラーが発生したアイテムは展開して見やすくする

            info.shown = true;
        }

        updateInfo.second = false;
    }
}

std::vector<ValidationErrorInfo> FileTree::processJsonBuffer(const QString& input)
{
    std::vector<ValidationErrorInfo> parsedErrors;
    int currentPos = 0;
    const int inputLength = input.length();

    while(currentPos < inputLength)
    {
        int startIndex = input.indexOf('{', currentPos);
        if(startIndex == -1)
        {
            break;
        }

        int depth = 0;
        bool inString = false;
        bool isEscaped = false;
        int endIndex = -1;

        for(int i = startIndex; i < inputLength; ++i)
        {
            QChar character = input.at(i);
            if(inString == true)
            {
                if(isEscaped == true)
                {
                    isEscaped = false;
                }
                else
                {
                    if(character == '\\')
                    {
                        isEscaped = true;
                    }
                    else if(character == '"')
                    {
                        inString = false;
                    }
                }
            }
            else
            {
                if(character == '"')
                {
                    inString = true;
                }
                else if(character == '{')
                {
                    ++depth;
                }
                else if(character == '}')
                {
                    --depth;
                    if(depth == 0)
                    {
                        endIndex = i;
                        break;
                    }
                }
            }
        }

        if(endIndex == -1)
        {
            break;
        }

        QString jsonStr = input.mid(startIndex, endIndex - startIndex + 1).trimmed();

        QJsonParseError parseError;
        QJsonDocument jsonDocument = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
        if(parseError.error != QJsonParseError::NoError)
        {
            qWarning() << "JSON parse error:" << parseError.errorString() << "in:" << jsonStr;
        }
        else if(jsonDocument.isObject() == false)
        {
            qWarning() << "JSON is not an object:" << jsonStr;
        }
        else
        {
            QJsonObject jsonObject = jsonDocument.object();
            ValidationErrorInfo errorInfo;

            this->errorInfoIndex = this->errorInfoIndex + 1;
            errorInfo.id = this->errorInfoIndex;

            if(jsonObject.contains("Type") == true)
            {
                int typeValue = jsonObject.value("Type").toInt();
                if(ValidationErrorInfo::Error <= typeValue && typeValue < ValidationErrorInfo::Invalid)
                {
                    errorInfo.type = static_cast<ValidationErrorInfo::ErrorType>(typeValue);
                }
                else
                {
                    errorInfo.type = ValidationErrorInfo::Invalid;
                }
            }

            if(jsonObject.contains("Summary") == true)
            {
                int summaryValue = jsonObject.value("Summary").toInt();
                if(ValidationErrorInfo::None < summaryValue && summaryValue < ValidationErrorInfo::Max)
                {
                    errorInfo.summary = static_cast<ValidationErrorInfo::ErrorSummary>(summaryValue);
                }
                else
                {
                    errorInfo.summary = ValidationErrorInfo::None;
                }
            }
            if(jsonObject.contains("File") == true)
            {
                errorInfo.filePath = jsonObject.value("File").toString();
            }
            if(jsonObject.contains("Row") == true)
            {
                errorInfo.row = static_cast<size_t>(jsonObject.value("Row").toInteger());
            }
            if(jsonObject.contains("Width") == true)
            {
                errorInfo.width = jsonObject.value("Width").toInt();
            }
            if(jsonObject.contains("Language") == true)
            {
                errorInfo.language = jsonObject.value("Language").toString();
            }
            if(jsonObject.contains("Details") == true)
            {
                errorInfo.detail = jsonObject.value("Details").toString();
            }

            parsedErrors.emplace_back(std::move(errorInfo));
        }

        currentPos = endIndex + 1;
    }

    return parsedErrors;
}
