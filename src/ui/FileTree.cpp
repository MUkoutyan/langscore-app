#include "FileTree.h"
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHeaderView>
#include <QThread>
#include <QLineEdit>
#include <QToolButton>
#include "../settings.h"
#include "../csv.hpp"
#include "../utility.hpp"
#include "GraphicsImageLoader.h"


namespace {
    QTreeWidgetItem* getTopLevelItem(QTreeWidgetItem* item) {
        while(item) {
            if(item->parent() == nullptr) { return item; }
            item = item->parent();
        }
        return nullptr;
    }

    QString getFileName(QTreeWidgetItem* item) {
        assert(1 < item->columnCount());
        return item->data(FileTree::TreeColIndex::Name, Qt::UserRole).toString();
    }
    QString getScriptName(QTreeWidgetItem* item) {
        assert(1 < item->columnCount());
        return item->text(FileTree::TreeColIndex::Name);
    }
}

FileTree::FileTree(ComponentBase* component, std::weak_ptr<LoadFileManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent)
    , loadFileManager(std::move(loadFileManager))
    , treeWidget(new QTreeWidget(this))
    , _suspendHistory(false)
    , graphicsImageLoader(nullptr)
    , graphicsLoaderThread(nullptr)
{

    this->treeWidget->header()->setVisible(false);
    this->treeWidget->setColumnCount(2);

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
    this->setupMainTree();
    this->setupScriptTree();
    this->setupGraphicsTree();
}

void FileTree::setupMainTree() 
{
    const auto analyzeFolder = this->setting->analyzeDirectoryPath();
    auto files = loadFileManager.lock()->getBasicFileList();

    auto mainItem = new QTreeWidgetItem();
    mainItem->setText(0, "Main");
    mainItem->setData(0, Qt::UserRole, TreeItemType::Main);
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
        child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);

        auto fileName = file.fileName;
        if(file.mapName.isEmpty() == false) {
            fileName += " (" + file.mapName + ")";
        }
        child->setText(TreeColIndex::Name, fileName);
        child->setData(TreeColIndex::Name, Qt::UserRole, file.fileName);
        child->setForeground(TreeColIndex::Name, Qt::white);

        const auto& basicDataInfo = this->setting->writeObj.basicDataInfo;
        auto result = std::find_if(basicDataInfo.cbegin(), basicDataInfo.cend(), [&file](const auto& x) {
            return x.name.contains(file.fileName);
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

    auto scriptFiles = loadFileManager.lock()->getScriptFileList();
    auto scriptExtention = settings::scriptExt(setting->projectType);
    for(const auto& l : scriptFiles)
    {
        const auto& scriptFileName = l.fileName;
        const auto& scriptName = l.scriptName;
        if(scriptName == "_NONAME_") {
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
            //チェックを外すとこのスクリプトを翻訳から除外します。
            child->setToolTip(TreeColIndex::CheckBox, tr("Unchecking the box excludes this script from translation."));
            child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
        }
        else {
            child->setFlags(Qt::ItemIsSelectable);
        }
        child->setText(TreeColIndex::Name, scriptName);
        child->setData(TreeColIndex::Name, Qt::UserRole, langscore::withoutExtension(scriptFileName));

        const auto& scriptList = this->setting->writeObj.scriptInfo;
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&scriptFileName](const auto& script) {
            return script.name.contains(scriptFileName);
            });
        if(result != scriptList.cend())
        {
            if(result->ignore) {
                child->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked);
            }
            else if(result->ignorePoint.empty() == false) {
                child->setCheckState(TreeColIndex::CheckBox, Qt::PartiallyChecked);
            }
            else {
                child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
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

    auto graphList = loadFileManager.lock()->getGraphicsFileList();
    // Map: full path -> QTreeWidgetItem*
    QMap<QString, QTreeWidgetItem*> folderItemMap;
    folderItemMap[""] = graphicsRootItem; // root 

    graphicsItemMap.clear();

    auto treeItemHeight = this->treeWidget->sizeHintForRow(0);

    for(const auto& graphInfo : graphList)
    {
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
                parentItem->addChild(dirItem);
                folderItemMap[currentPath] = dirItem;
            }
            parentItem = folderItemMap[currentPath];
        }

        // Add image item to the deepest directory node
        auto pictureFileName = graphInfo.fileName;
        auto* child = new QTreeWidgetItem();
        child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
        child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
        child->setText(TreeColIndex::Name, pictureFileName);
        child->setData(TreeColIndex::Name, Qt::UserRole, graphInfo.filePath);
        child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
        child->setForeground(0, Qt::white);

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
    if(treeType == TreeItemType::Main) {
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
    if(treeType == TreeItemType::Main)
    {
        auto fileName = ::getFileName(_item);
        const auto& basicData = this->setting->fetchBasicDataInfo(fileName);
        auto treeCheckState = basicData.ignore ? Qt::Unchecked : Qt::Checked;
        std::vector<QUndoCommand*> result;
        for(auto item : selectedItems) {
            if(newCheckVal == treeCheckState) { continue; }
            result.emplace_back(new FileTreeUndo(this, item, newCheckVal, treeCheckState));
        }

        NotifyTreeUndoCommand(std::move(result), tr("Main Tree Change Enable State"));
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
        else if(scriptList.ignorePoint.empty() == false) {
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

    // --- 追加: LoadFileManagerの状態も更新 ---
    if(treeType == TreeItemType::Script) {
        auto fileName = ::getFileName(item);
        if(auto manager = loadFileManager.lock()) {
            manager->setCheckState(fileName, check);
        }
    }
    // ---------------------------------------

    if(treeType == TreeItemType::Main) {
        writeToBasicDataListSetting(item, ignore);
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
        emit scriptTreeItemCheckChanged(scriptName, check);
    }
    // -----------------------------------------
}

void FileTree::writeToBasicDataListSetting(QTreeWidgetItem* item, bool ignore)
{
    auto fileName = ::getFileName(item);
    auto& info = this->setting->fetchBasicDataInfo(fileName);
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

void FileTree::onChangeScriptTableItemCheck(QString scriptName, Qt::CheckState)
{
    ////チェックしたアイテムに関連している、テーブル側のアイテムの色を変える
    //auto rows = fetchScriptTableSameFileRows(scriptName);
    //auto treeItem = fetchScriptTreeSameFileItem(scriptName);

    //auto tableTextColorForState = this->getColorTheme().getTextColorForState();
    //const QSignalBlocker scriptTableBlocker(this->ui->treeWidget);
    //if(treeItem && treeItem->checkState(0) == Qt::Unchecked)
    //{
    //    for(auto row : rows) {
    //        setTableItemTextColor(row, tableTextColorForState[Qt::Unchecked]);
    //    }
    //}
    //else
    //{
    //    for(auto row : rows)
    //    {
    //        auto checkItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck);
    //        if(checkItem == nullptr) { continue; }
    //        setTableItemTextColor(row, tableTextColorForState[checkItem->checkState()]);
    //    }
    //}

    ////ツリー側のチェック状態を変更
    //if(treeItem)
    //{
    //    //同じスクリプトが全て無視されたかをチェック
    //    auto treeCheckState = getTreeCheckStateBasedOnTable(scriptName);
    //    //スクリプトの無視状態が最優先なので、チェック無しの場合は何もしない。
    //    if(treeItem->checkState(0) != Qt::Unchecked) {
    //        treeItem->setCheckState(0, treeCheckState);
    //        //ヒストリーを介さない方法で変更するため、ここで設定ファイルへの書き込みを行う
    //        writeToScriptListSetting(treeItem, check == Qt::Unchecked);
    //    }
    //    treeItem->setForeground(1, tableTextColorForState[treeCheckState]);
    //}

    //this->update();
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
        item->setIcon(column, QIcon(QPixmap::fromImage(image)));
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

    searchLayout->addWidget(searchLineEdit);

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
        topItem->setHidden(false);

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

    item->setHidden(false);
    item->setForeground(TreeColIndex::Name, getColorTheme().getItemTextColor());

    // 子アイテムも再帰的に処理
    for(int i = 0; i < item->childCount(); i++) {
        resetItemVisibility(item->child(i));
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