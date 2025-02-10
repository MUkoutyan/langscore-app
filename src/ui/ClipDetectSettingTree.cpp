// ClipDetectSettingTree.cpp
#include "ClipDetectSettingTree.h"
#include "../utility.hpp"
#include "../csv.hpp"
#include <QComboBox>
#include <QSpinBox>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QDir>

using namespace langscore;

constexpr static int ValidateTypeRole = Qt::UserRole + 1;
constexpr static int ValidateNumber = ValidateTypeRole + 1;


std::array<QString, 3> textValidateModeText = {
    QObject::tr("Not Detect"),
    QObject::tr("Text Length"),
    QObject::tr("Text Width"),
};

ClipDetectSettingTreeDelegate::ClipDetectSettingTreeDelegate(QObject* parent) : QStyledItemDelegate(parent) {}

//QWidget* ClipDetectSettingTreeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const 
//{
//    if (index.column() == 1) {
//        QComboBox* editor = new QComboBox(parent);
//        editor->addItems(QStringList{textValidateModeText.begin(), textValidateModeText.end()});
//        connect(editor, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, editor, index](int value) {
//            emit detectModeIndexChanged(index, value);
//        });
//        connect(editor, &QComboBox::currentTextChanged, editor, &QComboBox::close);
//        return editor;
//    } else if (index.column() == 2) {
//        QSpinBox* editor = new QSpinBox(parent);
//        editor->setMinimum(0);
//        editor->setMaximum(200);
//        return editor;
//    }
//    return QStyledItemDelegate::createEditor(parent, option, index);
//}

QWidget* ClipDetectSettingTreeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.column() >= 1) {
        QWidget* editor = new QWidget(parent);
        QHBoxLayout* layout = new QHBoxLayout(editor);
        layout->setContentsMargins(0, 0, 0, 0);

        QComboBox* comboBox = new QComboBox(editor);
        comboBox->addItems(QStringList{textValidateModeText.begin(), textValidateModeText.end()});
        layout->addWidget(comboBox);

        QSpinBox* spinBox = new QSpinBox(editor);
        spinBox->setMinimum(0);
        spinBox->setMaximum(200);
        layout->addWidget(spinBox);

        connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, comboBox, spinBox, index](int value) {
            emit detectModeIndexChanged(index, value);
            spinBox->setEnabled(value != 0);
        });

        return editor;
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
}


//void ClipDetectSettingTreeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
//    if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(editor)) {
//        spinBox->setValue(index.model()->data(index, ValidateNumber).toInt());
//    } else if (QComboBox* comboBox = qobject_cast<QComboBox*>(editor)) {
//        comboBox->setCurrentText(textValidateModeText[index.model()->data(index, ValidateTypeRole).toInt()]);
//        comboBox->showPopup();
//    } else {
//        QStyledItemDelegate::setEditorData(editor, index);
//    }
//}

void ClipDetectSettingTreeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const {
    if(index.column() >= 1) {
        QComboBox* comboBox = editor->findChild<QComboBox*>();
        QSpinBox* spinBox = editor->findChild<QSpinBox*>();

        if(comboBox && spinBox) {
            auto comboBoxIndex = index.model()->data(index, ValidateTypeRole).toInt();
            comboBox->setCurrentText(textValidateModeText[comboBoxIndex]);
            spinBox->setValue(index.model()->data(index, ValidateNumber).toInt());
            if(comboBoxIndex == 0) {
                spinBox->setEnabled(false);
            }
            //comboBox->showPopup();
        }
    }
    else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

//void ClipDetectSettingTreeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const 
//{
//    if (QSpinBox* spinBox = qobject_cast<QSpinBox*>(editor)) {
//        model->setData(index, spinBox->value(), ValidateNumber);
//    } else if (QComboBox* comboBox = qobject_cast<QComboBox*>(editor)) {
//        model->setData(index, comboBox->currentIndex(), ValidateTypeRole);
//    } else {
//        QStyledItemDelegate::setModelData(editor, model, index);
//    }
//}

void ClipDetectSettingTreeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    if(index.column() >= 1) {
        QComboBox* comboBox = editor->findChild<QComboBox*>();
        QSpinBox* spinBox = editor->findChild<QSpinBox*>();

        if(comboBox && spinBox) {
            model->setData(index, comboBox->currentIndex(), ValidateTypeRole);
            model->setData(index, spinBox->value(), ValidateNumber);
        }
    }
    else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

QSize ClipDetectSettingTreeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    size.setHeight(24);
    if(index.column() == 0) {
        size.setWidth(160);
    }
    else {
        size.setWidth(320);
    }
    return size;
}

void ClipDetectSettingTreeDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    editor->setGeometry(option.rect);
}

void ClipDetectSettingTreeDelegate::detectModeIndexChanged(const QModelIndex& index, int value) const
{
}

ClipDetectSettingTreeModel::ClipDetectSettingTreeModel(std::shared_ptr<settings> settings, QUndoStack* history, ClipDetectSettingTree* parent)
    : QAbstractItemModel(parent), tree(parent), history(history), setting(settings), rootItem(nullptr)

{
    //setupClipDetectTree();
}

QModelIndex ClipDetectSettingTreeModel::index(int row, int column, const QModelIndex& parent) const
{
    if(hasIndex(row, column, parent) == false) {
        return QModelIndex();
    }

    TreeNode* parentItem = getItem(parent);
    TreeNode* childItem = parentItem->children.at(row).get();

    if(childItem) {
        return createIndex(row, column, childItem);
    }
    else {
        return QModelIndex();
    }
}

QModelIndex ClipDetectSettingTreeModel::parent(const QModelIndex& childIndex) const
{
    // インデックスが無効な場合、空のQModelIndexを返す
    if(childIndex.isValid() == false) {
        return QModelIndex();
    }

    // インデックスから子アイテムを取得
    TreeNode* childItem = getItem(childIndex);
    // 子アイテムの親アイテムを取得
    TreeNode* parentItem = childItem->parent;

    // 親アイテムがnullの場合、空のQModelIndexを返す
    if(parentItem == nullptr) {
        return QModelIndex();
    }

    // 親アイテムの親アイテム（祖父母アイテム）を取得
    TreeNode* grandParentItem = parentItem->parent;
    // 祖父母アイテムがnullの場合、空のQModelIndexを返す
    if(grandParentItem == nullptr) {
        return QModelIndex();
    }

    // 祖父母アイテムの子供リストから親アイテムを検索
    auto it = std::ranges::find_if(grandParentItem->children,[parentItem](const auto& child) {
        return child.get() == parentItem; 
    });

    // 親アイテムが見つかった場合、そのインデックスを返す
    if(it != grandParentItem->children.end())
    {
        int row = std::distance(grandParentItem->children.begin(), it);
        return createIndex(row, 0, parentItem);
    }

    // 親アイテムが見つからなかった場合、空のQModelIndexを返す
    return QModelIndex();
}


int ClipDetectSettingTreeModel::rowCount(const QModelIndex& parent) const
{
    TreeNode* parentItem = getItem(parent);
    if(parentItem == nullptr) { return 0; }
    return static_cast<int>(parentItem->children.size());
}

int ClipDetectSettingTreeModel::columnCount(const QModelIndex& parent) const
{
    return 1 + enableLangNames.size();
}

QVariant ClipDetectSettingTreeModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    TreeNode* item = getItem(index);

    if(role == Qt::DisplayRole) {
        if(index.column() == 0) {
            return item->name;
        }
        else 
        {
            auto langName = this->getLangName(index);
            const auto& validateInfo = item->getValidateTextInfo(langName);
            QString text = "";
            if(validateInfo.mode == settings::ValidateTextMode::TextCount) {
                text = tr("Text Count : %1").arg(validateInfo.count);
            }
            else if(validateInfo.mode == settings::ValidateTextMode::TextWidth) {
                text = tr("Text Width : %1").arg(validateInfo.width);
            }
            else {
                text = tr("Not Detect");
            }
            return text;
        }
    }
    else if(role == ValidateNumber) 
    {
        auto langName = this->getLangName(index);
        const auto& validateInfo = item->getValidateTextInfo(langName);
        if(validateInfo.mode == settings::ValidateTextMode::TextCount) {
            return validateInfo.count;
        }
        else if(validateInfo.mode == settings::ValidateTextMode::TextWidth) {
            return validateInfo.width;
        }
    }
    else if(role == ValidateTypeRole) {
        auto langName = this->getLangName(index);
        return static_cast<int>(item->getValidateTextInfo(langName).mode);
    }

    return QVariant();
}

bool ClipDetectSettingTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid()) {
        return false;
    }

    TreeNode* item = getItem(index);

    if(role == ValidateTypeRole) {
        auto oldValue = static_cast<settings::ValidateTextMode>(data(index, role).toInt());
        auto newValue = static_cast<settings::ValidateTextMode>(value.toInt());
        if(oldValue != newValue) {
            history->push(new DetectTreeUndo(this, index, role, oldValue, newValue));
            emit dataChanged(index, index, {role});
            return true;
        }
    }
    else if(role == ValidateNumber) {
        auto oldValue = data(index, role).toInt();
        if(oldValue != value.toInt()) {
            history->push(new DetectTreeUndo(this, index, role, oldValue, value));
            emit dataChanged(index, index, {role});
            return true;
        }
    }

    return QAbstractItemModel::setData(index, value, role);
}

//bool ClipDetectSettingTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
//{
//    if(!index.isValid()) {
//        return false;
//    }
//
//    TreeNode* item = getItem(index);
//
//    int col = index.column();
//    if(role == Qt::EditRole)
//    {
//        if(col == 1) {
//            auto oldValue = static_cast<settings::ValidateTextMode>(data(index, role).toInt());
//            auto newValue = static_cast<settings::ValidateTextMode>(value.toInt());
//            if(oldValue != newValue) {
//                history->push(new DetectTreeUndo(this, index, oldValue, newValue));
//            }
//            return true;
//        }
//        else if(col == 2) {
//            auto oldValue = data(index, role);
//            if(item->validateType.mode == settings::ValidateTextMode::TextCount) {
//                if(oldValue != value) {
//                    history->push(new DetectTreeUndo(this, index, oldValue, value));
//                }
//                return true;
//            }
//            else if(item->validateType.mode == settings::ValidateTextMode::TextWidth) {
//                if(oldValue != value) {
//                    history->push(new DetectTreeUndo(this, index, oldValue, value));
//                }
//                return true;
//            }
//        }
//    }
//    return QAbstractItemModel::setData(index, value, role);
//}


Qt::ItemFlags ClipDetectSettingTreeModel::flags(const QModelIndex& index) const
{
    if(index.isValid() == false) {
        return Qt::NoItemFlags;
    }
    if(index.column() == 1 || index.column() == 2) {
        return (Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
    }

    return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
}

QVariant ClipDetectSettingTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal) {
        return QVariant();
    }
    if(role == Qt::DisplayRole) {
        if(section == 0) {
            return QVariant();
        }
        else if((section-1) < enableLangNames.size())
        {
            return enableLangNames[section-1];
        }
    }
    return QVariant();
}

void ClipDetectSettingTreeModel::setupClipDetectTree()
{
    QStringList currentEnableLangNames;
    for(auto& langs : this->setting->languages) {
        if(langs.enable) {
            currentEnableLangNames.emplace_back(langs.languageName);
        }
    }
    if(enableLangNames == currentEnableLangNames) {
        return;
    }
    enableLangNames = std::move(currentEnableLangNames);

    auto csvList = QDir{setting->analyzeDirectoryPath()}.entryInfoList({"*.csv"});

    // 解析CSVから文字列のタイプを取得する。
    std::map<QString, std::set<QString>> csvNameTypeMap;
    std::set<QString> batchNameTypeList;
    for(const auto& csvPath : csvList)
    {
        auto fileName = langscore::withoutExtension(csvPath.fileName());

        auto csvContents = langscore::readCsv(csvPath.absoluteFilePath());
        std::set<QString> nameTypeList;
        csvContents.erase(csvContents.begin()); // ヘッダーの削除
        for(auto& row : csvContents) {
            if(row.size() == 2) {
                nameTypeList.emplace(row[1]);
                batchNameTypeList.emplace(row[1]);
            }
        }
        csvNameTypeMap[fileName] = nameTypeList;
    }

    settings::TextValidationLangMap langMap;
    for(auto lang : this->enableLangNames) {
        langMap[lang] = {};
    }

    this->beginResetModel();
    rootItem.reset(new TreeNode());
    // Batchという親アイテムを作成
    auto batchTopItem = std::make_unique<TreeNode>();
    batchTopItem->name = "Batch assignment";
    batchTopItem->type = TreeNode::Batch;
    batchTopItem->validateType = langMap;
    batchTopItem->parent = rootItem.get();
    rootItem->children.emplace_back(std::move(batchTopItem));
    auto batchTopItemPtr = rootItem->children.back().get();

    // Batch内にnameTypeListのツリーを作成
    for(auto& nameType : batchNameTypeList)
    {
        auto batchItem = std::make_unique<TreeNode>();
        batchItem->name = nameType;
        batchItem->type = TreeNode::Batch;
        batchItem->validateType = langMap;
        batchItem->parent = batchTopItemPtr;
        batchTopItemPtr->children.emplace_back(std::move(batchItem));
    }

    // Filesという親アイテムを作成
    auto filesTopItem = std::make_unique<TreeNode>();
    filesTopItem->name = "Files";
    filesTopItem->type = TreeNode::Files;
    filesTopItem->validateType = langMap;
    filesTopItem->parent = rootItem.get();
    rootItem->children.emplace_back(std::move(filesTopItem));
    auto filesTopItemPtr = rootItem->children.back().get();

    TreeNode* mapsTopItemPtr = nullptr;
    TreeNode* mapsItemPtr = nullptr;
    for(auto csvPath : csvList)
    {
        auto fileName = langscore::withoutExtension(csvPath.fileName());
        if(fileName == "MapInfos") { continue; }

        TreeNode* topItemPtr = nullptr;
        if(fileName.startsWith("Map"))
        {
            if(mapsTopItemPtr == nullptr) {
                auto mapsTopItem = std::make_unique<TreeNode>();
                mapsTopItem->name = "Maps";
                mapsTopItem->type = TreeNode::Files;
                mapsTopItem->validateType = langMap;
                mapsTopItem->parent = filesTopItemPtr;
                filesTopItemPtr->children.emplace_back(std::move(mapsTopItem));
                mapsTopItemPtr = filesTopItemPtr->children.back().get();
            }
            auto topItem = std::make_unique<TreeNode>();
            topItem->name = fileName;
            topItem->type = TreeNode::Files;
            topItem->validateType = langMap;
            topItem->parent = mapsTopItemPtr;
            mapsTopItemPtr->children.emplace_back(std::move(topItem));
            topItemPtr = mapsTopItemPtr->children.back().get();
        }
        else {
            auto topItem = std::make_unique<TreeNode>();
            topItem->name = fileName;
            topItem->type = TreeNode::Files;
            topItem->validateType = langMap;
            topItem->parent = filesTopItemPtr;
            filesTopItemPtr->children.emplace_back(std::move(topItem));
            topItemPtr = filesTopItemPtr->children.back().get();
        }

        auto& csvData = this->setting->getValidationCsvDataRef(fileName);
        const auto& nameTypeList = csvNameTypeMap.at(fileName);
        for(auto& nameType : nameTypeList)
        {
            auto childItem = std::make_unique<TreeNode>();
            childItem->name = nameType;
            childItem->type = TreeNode::Files;
            if(csvData.find(nameType) == csvData.end()) {
                csvData[nameType] = langMap;
            }
            childItem->validateType = csvData.at(nameType);
            childItem->settingData = &csvData[nameType];
            childItem->parent = topItemPtr;
            topItemPtr->children.emplace_back(std::move(childItem));
        }
    }

    this->endResetModel();
}

QModelIndex ClipDetectSettingTreeModel::getLastDescendantIndex(const QModelIndex& parentIndex, int column) const {
    QModelIndex last = parentIndex;
    while(rowCount(last) > 0) {
        last = index(rowCount(last) - 1, column, last);
    }
    return last;
}

void ClipDetectSettingTreeModel::findFilesNodesByName(TreeNode* current, const QString& targetName, std::vector<TreeNode*>& result) const {
    if(current->type == TreeNode::Files && current->name == targetName) {
        result.push_back(current);
    }
    for(const auto& child : current->children) {
        findFilesNodesByName(child.get(), targetName, result);
    }
}

ClipDetectSettingTreeModel::TreeNode* ClipDetectSettingTreeModel::getItem(const QModelIndex& index) const
{
    if(index.isValid()) {
        return static_cast<TreeNode*>(index.internalPointer());
    }
    return this->getRootItem();
}

ClipDetectSettingTreeModel::TreeNode* ClipDetectSettingTreeModel::getRootItem() const
{
    return this->rootItem.get();
}

QString ClipDetectSettingTreeModel::getLangName(const QModelIndex& index) const
{
    if(index.isValid() == false) { return QString(); }
    auto col = index.column();
    if(col == 0) { return QString(); }
    auto namesIndex = col - 1;
    if(this->enableLangNames.size() <= namesIndex) { return QString(); }
    return this->enableLangNames[namesIndex];
}


ClipDetectSettingTree::ClipDetectSettingTree(std::shared_ptr<settings> setting, QUndoStack* history, QWidget* parent)
    : QTreeView(parent) 
    , setting(setting)
    , model(new ClipDetectSettingTreeModel(setting, history, this))
{
    this->setModel(model);

    this->setAlternatingRowColors(true);
    this->setAnimated(true);

    ClipDetectSettingTreeDelegate* delegate = new ClipDetectSettingTreeDelegate(this);
    this->setItemDelegateForColumn(1, delegate); // 2列目にデリゲートを設定
    this->setItemDelegateForColumn(2, delegate); // 3列目にデリゲートを設定

    connect(history, &QUndoStack::indexChanged, this, [this](int) { this->update(); });
}

void ClipDetectSettingTree::setup()
{
    this->model->setupClipDetectTree();
}

void ClipDetectSettingTree::mousePressEvent(QMouseEvent* event) 
{
    QModelIndex index = indexAt(event->pos());

    if(index.isValid()) {
        // 前回クリックしたセルと異なる場合、最初のクリックでは確定のみ行う
        if(index != lastClickedIndex) {
            // QComboBox の場合、即時確定する
            if(indexWidget(lastClickedIndex) != nullptr) {
                qDebug() << "close";
                closePersistentEditor(lastClickedIndex); // 現在のエディタを閉じる（確定する）
                lastClickedIndex = index;
                return; // このクリックでは新しいエディタを開かない
            }
        }

        // 2回目のクリック（または同じセルのクリック）でエディタを開く
        lastClickedIndex = index;
        edit(index);
    }

    QTreeView::mousePressEvent(event);
}


void ClipDetectSettingTreeModel::DetectTreeUndo::undo()
{
    TreeNode* item = parent->getItem(index);
    if(item == nullptr) { return; }

    if(role == ValidateTypeRole) {
        this->SetMode(item, static_cast<settings::ValidateTextMode>(oldValue.toInt()), parent->history);
    }
    else if(role == ValidateNumber) {
        auto langName = parent->getLangName(index);
        if(item->getValidateTextInfo(langName).mode == settings::ValidateTextMode::TextCount) {
            this->SetCount(item, oldValue.toInt(), parent->history);
        }
        else if(item->getValidateTextInfo(langName).mode == settings::ValidateTextMode::TextWidth) {
            this->SetWidth(item, oldValue.toInt(), parent->history);
        }
    }
    QModelIndex lastIndex = parent->getLastDescendantIndex(index, index.column());
    emit parent->dataChanged(index, lastIndex);
    parent->tree->viewport()->update();

}

void ClipDetectSettingTreeModel::DetectTreeUndo::redo()
{
    TreeNode* item = parent->getItem(index);
    if(item == nullptr) { return; }

    if(role == ValidateTypeRole) {
        this->SetMode(item, static_cast<settings::ValidateTextMode>(newValue.toInt()), parent->history);
    }
    else if(role == ValidateNumber) {
        auto langName = parent->getLangName(index);
        if(item->getValidateTextInfo(langName).mode == settings::ValidateTextMode::TextCount) {
            this->SetCount(item, newValue.toInt(), parent->history);
        }
        else if(item->getValidateTextInfo(langName).mode == settings::ValidateTextMode::TextWidth) {
            this->SetWidth(item, newValue.toInt(), parent->history);
        }
    }
    QModelIndex lastIndex = parent->getLastDescendantIndex(index, index.column());
    emit parent->dataChanged(index, lastIndex);
    parent->tree->viewport()->update();
}



void ClipDetectSettingTreeModel::DetectTreeUndo::SetMode(ClipDetectSettingTreeModel::TreeNode* node, settings::ValidateTextMode value, QUndoStack* history)
{
    if(node == nullptr) { return; }


    auto langName = parent->getLangName(index);
    if(node->type == TreeNode::Batch) 
    {
        node->getValidateTextInfo(langName).mode = value;

        auto rootItem = parent->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            parent->findFilesNodesByName(child.get(), node->name, matchingNodes);
            for(TreeNode* filesNode : matchingNodes) {
                filesNode->getValidateTextInfo(langName).mode = value;
                if(filesNode->settingData) {
                    (*filesNode->settingData)[langName].mode = value;
                }
            }
        }
    }
    else if(node->type == TreeNode::Files) 
    {
        auto oldValue = node->getValidateTextInfo(langName).mode;
        if(value != oldValue) {
            node->getValidateTextInfo(langName).mode = value;
            if(node->settingData) {
                (*node->settingData)[langName].mode = value;
            }
        }
        for(auto& child : node->children) {
            SetMode(child.get(), value, history);
        }
    }

}

void ClipDetectSettingTreeModel::DetectTreeUndo::SetCount(ClipDetectSettingTreeModel::TreeNode* node, int value, QUndoStack* history)
{
    if(node == nullptr) { return; }

    auto langName = parent->getLangName(index);
    if(node->type == TreeNode::Batch) 
    {
        node->getValidateTextInfo(langName).count = value;
        auto rootItem = parent->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            parent->findFilesNodesByName(child.get(), node->name, matchingNodes);
            for(TreeNode* filesNode : matchingNodes) {
                filesNode->getValidateTextInfo(langName).count = value;
                if(filesNode->settingData) {
                    (*filesNode->settingData)[langName].count = value;
                }
            }
        }
    }
    else if(node->type == TreeNode::Files) 
    {
        auto oldValue = node->getValidateTextInfo(langName).count;
        if(value != oldValue) {
            node->getValidateTextInfo(langName).count = value;
            if(node->settingData) {
                (*node->settingData)[langName].count = value;
            }
        }
        for(auto& child : node->children) {
            SetCount(child.get(), value, history);
        }
    }
}

void ClipDetectSettingTreeModel::DetectTreeUndo::SetWidth(ClipDetectSettingTreeModel::TreeNode* node, int value, QUndoStack* history)
{
    if(node == nullptr) { return; }

    auto langName = parent->getLangName(index);
    if(node->type == TreeNode::Batch) 
    {
        node->getValidateTextInfo(langName).width = value;
        auto rootItem = parent->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            parent->findFilesNodesByName(child.get(), node->name, matchingNodes);
            for(TreeNode* filesNode : matchingNodes) {
                filesNode->getValidateTextInfo(langName).width = value;
                if(filesNode->settingData) {
                    (*filesNode->settingData)[langName].width = value;
                }
            }
        }
    }
    else if(node->type == TreeNode::Files) {
        auto oldValue = node->getValidateTextInfo(langName).width;
        if(value != oldValue) {
            node->getValidateTextInfo(langName).width = value;
            if(node->settingData) {
                (*node->settingData)[langName].width = value;
            }
        }
        for(auto& child : node->children) {
            SetWidth(child.get(), value, history);
        }
    }
}

void ClipDetectSettingTreeModel::DetectTreeUndo::propagateBatchChange(TreeNode* batchNode)
{
    // ルートの直下に Batch と Files がある前提なので、
    // Files 側のトップアイテムを取得する
}

settings::ValidateTextInfo& ClipDetectSettingTreeModel::TreeNode::getValidateTextInfo(QString langName)
{
    //有効な言語追加時の追従が大変なのでここで追加もする(良くはない)
    if(this->validateType.find(langName) == this->validateType.end()) {
        this->validateType[langName] = {};
    }
    return this->validateType.at(langName);
}
