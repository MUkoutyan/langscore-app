// ClipDetectSettingTree.cpp
#include "ClipDetectSettingTree.h"
#include "../utility.hpp"
#include "../csv.hpp"
#include <QComboBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QDir>
#include <QDialog>
#include <QToolButton>
#include <QPushButton>
#include <QLabel>
#include <QPainter>

#include <unordered_map>

using namespace langscore;

constexpr static int ValidateInfo = Qt::UserRole + 1;
constexpr static int OtherCtrlChar = ValidateInfo + 1;

Q_DECLARE_METATYPE(settings::ValidateTextInfo);


ClipDetectSettingTreeDelegate::ClipDetectSettingTreeDelegate(ClipDetectSettingTreeModel* model, QObject* parent) 
    : QStyledItemDelegate(parent)
    , model(model)
    , textValidateModeText({
        tr("Not Detect"),
        tr("Char Length"),
        tr("Text Width"),
    })
{
}


QWidget* ClipDetectSettingTreeDelegate::createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    if(index.column() >= 1) 
    {
        auto* item = model->getItem(index);

        if(item && item->type != ClipDetectSettingTreeModel::TreeNode::Type::Other)
        {
            QWidget* editor = new QWidget(parent);
            QHBoxLayout* layout = new QHBoxLayout(editor);
            layout->setContentsMargins(0, 0, 0, 0);

            QComboBox* comboBox = new QComboBox(editor);
            comboBox->addItems(QStringList{textValidateModeText.begin(), textValidateModeText.end()});
            layout->addWidget(comboBox);

            QSpinBox* spinBox = new QSpinBox(editor);
            spinBox->setMinimum(0);
            spinBox->setMaximum(10000);
            layout->addWidget(spinBox);

            connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this, comboBox, spinBox, index](int value) {
                spinBox->setEnabled(value != 0);
            });
            //connect(spinBox, &QSpinBox::valueChanged, this, [this, comboBox, spinBox, index](int value) {
            //    qDebug() << "valueChanged : " << value;
            //});

            return editor;
        }
        else if(auto* parentItem = model->getItem(model->parent(index)))
        {
            if(parentItem->isGroup && parentItem->type == ClipDetectSettingTreeModel::TreeNode::Type::Other)
            {
                QDialog* dialog = new QDialog(parent);
                dialog->setWindowFlags(Qt::Popup);
                auto* tagWidget = new TagWidget(this->model->getSetting(), this->model->getUndoStack(), dialog);
                QVBoxLayout* layout = new QVBoxLayout(dialog);
                layout->setContentsMargins(0, 0, 0, 0);
                layout->setSpacing(0);
                layout->addWidget(tagWidget);
                dialog->setLayout(layout);

                return dialog;
            }
        }
    }
    return QStyledItemDelegate::createEditor(parent, option, index);
}


void ClipDetectSettingTreeDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const 
{
    if(index.column() >= 1) 
    {
        auto* item = model->getItem(index);

        if(item && item->type != ClipDetectSettingTreeModel::TreeNode::Type::Other)
        {
            QComboBox* comboBox = editor->findChild<QComboBox*>();
            QSpinBox* spinBox = editor->findChild<QSpinBox*>();

            if(comboBox && spinBox) {
                const auto& info = index.model()->data(index, ValidateInfo).value<settings::ValidateTextInfo>();
                comboBox->setCurrentText(textValidateModeText[info.mode]);
                spinBox->setValue(info.value());
                if(info.mode == settings::ValidateTextMode::Ignore) {
                    spinBox->setEnabled(false);
                }
            }
        }
        else if(auto* parentItem = model->getItem(model->parent(index)))
        {
            if(parentItem->isGroup && parentItem->type == ClipDetectSettingTreeModel::TreeNode::Type::Other)
            {
                auto dialog = qobject_cast<QDialog*>(editor);
                if(auto tagWidget = dialog->findChild<TagWidget*>())
                {
                    auto parentWidget = dialog->parentWidget();
                    const auto& ctrlCharList = index.model()->data(index, OtherCtrlChar).toStringList();
                    tagWidget->setup(ctrlCharList);
                }
            }
        }
    }
    else {
        QStyledItemDelegate::setEditorData(editor, index);
    }
}

void ClipDetectSettingTreeDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const
{
    if(1 <= index.column()) 
    {
        auto* item = this->model->getItem(index);

        if(item && item->type != ClipDetectSettingTreeModel::TreeNode::Type::Other)
        {
            QComboBox* comboBox = editor->findChild<QComboBox*>();
            QSpinBox* spinBox = editor->findChild<QSpinBox*>();

            if(comboBox && spinBox) {

                settings::ValidateTextInfo info;
                info.mode = static_cast<settings::ValidateTextMode>(comboBox->currentIndex());
                info.setValue(spinBox->value());
                model->setData(index, QVariant::fromValue(info), ValidateInfo);
            }
        }
        else if(auto* parentItem = this->model->getItem(model->parent(index)))
        {
            if(parentItem->isGroup && parentItem->type == ClipDetectSettingTreeModel::TreeNode::Type::Other)
            {
                auto dialog = qobject_cast<QDialog*>(editor);
                if(auto tagWidget = dialog->findChild<TagWidget*>())
                {
                    auto currentTagStrList = tagWidget->getTagList();
                    model->setData(index, currentTagStrList, OtherCtrlChar);
                }
            }
        }
    }
    else {
        QStyledItemDelegate::setModelData(editor, model, index);
    }
}

QSize ClipDetectSettingTreeDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    return size;
}

void ClipDetectSettingTreeDelegate::updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    if(auto dialog = qobject_cast<QDialog*>(editor)) {
        // ダイアログの位置を設定
        QRect cellRect = option.rect;
        QPoint globalPos = option.widget->mapToGlobal(cellRect.bottomLeft());
        dialog->move(globalPos);
    }
    else {
        editor->setGeometry(option.rect);
    }
}

void ClipDetectSettingTreeDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    auto* item = model->getItem(index);

    if(item && item->type == ClipDetectSettingTreeModel::TreeNode::Type::Other)
    {
        if(index.column() > 1)
        {
            // "control character"の列で2列目以降は描画しない
            return;
        }
        else if(index.column() == 1)
        {
            QString text = index.model()->data(index, Qt::DisplayRole).toString();
            painter->save();
            painter->setPen(option.palette.color(QPalette::Text));
            painter->drawText(option.rect, Qt::AlignLeft | Qt::AlignVCenter, text);
            painter->restore();
            return;
        }
    }

    // デフォルトの描画
    QStyledItemDelegate::paint(painter, option, index);
}

ClipDetectSettingTreeModel::ClipDetectSettingTreeModel(std::shared_ptr<settings> settings, QUndoStack* history, ClipDetectSettingTree* parent)
    : QAbstractItemModel(parent), tree(parent), history(history), setting(settings), rootItem(nullptr)
{
    typeNameTranslateList = {
       {QString("name"),             QObject::tr("name")},
       {QString("description"),      QObject::tr("description")},
       {QString("messageWithIcon"),  QObject::tr("messageWithIcon")},
       {QString("battleMessage"),    QObject::tr("battleMessage")},
       {QString("message"),          QObject::tr("message")},
       {QString("other"),            QObject::tr("other")},
    };

    connect(this->history, &QUndoStack::indexChanged, this, [this](int idx) {
        this->tree->viewport()->update();
    });
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

static bool HasNotDefaultParameter(ClipDetectSettingTreeModel::TreeNode const* const node)
{
    for(auto& [lang, infos] : node->validateLangMap) {
        if(infos != settings::ValidateTextInfo{}) {
            return true;
        }
    }

    for(const auto& child : node->children) {
        auto result = HasNotDefaultParameter(child.get());
        if(result) { return true; }
    }

    return false;
}

static bool IsChildrenSameParameters(QString langName, ClipDetectSettingTreeModel::TreeNode const* const node, settings::ValidateTextInfo& baseInfo, bool& first)
{
    for(const auto& child : node->children)
    {
        if(child->isGroup) {
            if(IsChildrenSameParameters(langName, child.get(), baseInfo, first) == false){
                return false;
            }
            continue;
        }
        if(child->validateLangMap.find(langName) != child->validateLangMap.end())
        {
            auto& info = child->validateLangMap[langName];
            if(first) {
                baseInfo = info;
                first = false;
                continue;
            }
            if(baseInfo != info) {
                return false;
            }
        }
    }
    return true;
}

QVariant ClipDetectSettingTreeModel::data(const QModelIndex& index, int role) const
{
    if(!index.isValid()) {
        return QVariant();
    }

    TreeNode* item = getItem(index);

    if(role == Qt::DisplayRole) 
    {
        if(index.column() == 0) 
        {
            if(item->children.empty()) {
                if(typeNameTranslateList.find(item->name) != typeNameTranslateList.end()) {
                    return typeNameTranslateList.at(item->name);
                }
            }
            else if(item->parent && item->parent->name == "Maps") 
            {
                int mapID = -1;
                if(QRegularExpression("Map\\d\\d\\d").match(item->name).hasMatch())
                {
                    auto fileID = item->name;
                    fileID.remove("Map");
                    bool isOK = false;
                    mapID = fileID.toInt(&isOK);
                    if(isOK == false) {
                        mapID = -1;
                    }
                    else {
                        auto result = std::find_if(mapInfosCsv.cbegin(), mapInfosCsv.cend(), [mapID](const auto& x) {
                            return x[0].toInt() == mapID;
                            });
                        if(result != mapInfosCsv.cend()) {
                            return item->name + "(" + (*result)[2] + ")";
                        }
                    }
                }
            }

            return item->name;
        }
        else 
        {
            if(item->isGroup && item->type == TreeNode::Batch) {
                return QVariant();
            }
            if(item->type == TreeNode::Other) 
            {
                if(item->children.empty() && index.column() == 1)
                {
                    return setting->validateObj.controlCharList.join(", ");
                }
                return QVariant();
            }
            const auto GetText = [](const settings::ValidateTextInfo& validateInfo) {
                QString text = "";
                if(validateInfo.mode == settings::ValidateTextMode::TextCount) {
                    text = tr("Text Length") + QString(" : %1").arg(validateInfo.count);
                }
                else if(validateInfo.mode == settings::ValidateTextMode::TextWidth) {
                    text = tr("Text Width") + QString(" : %1").arg(validateInfo.width);
                }
                else {
                    text = tr("Not Detect");
                }
                return text;
            };
            auto langName = this->getLangName(index);
            if(item->isGroup)
            {
                settings::ValidateTextInfo baseInfo;
                bool isFirst = true;
                auto sameItems = IsChildrenSameParameters(langName, item, baseInfo, isFirst);

                if(sameItems) {
                    return GetText(baseInfo);
                }
                else {
                    //複数の設定
                    return tr("Multiple settings");
                }
            }
            else 
            {
                const auto& validateInfo = item->getValidateTextInfo(langName);
                return GetText(validateInfo);
            }
        }
    }
    else if(role == Qt::SizeHintRole) 
    {
        auto defaultSize = QSize(400, 24);
        if(item->isGroup) {
            defaultSize.setHeight(double(defaultSize.height()) * 1.4);
            return defaultSize;
        }
        return defaultSize;
    }
    else if(role == Qt::FontRole)
    {
        if(index.column() == 0) {
            QFont font;
            auto result = HasNotDefaultParameter(item);
            font.setItalic(result);
            font.setBold(result);
            return font;
            
        }
    }
    else if(role == Qt::ForegroundRole)
    {
        if(index.column() != 0 && item->isGroup) {
            auto color = QColor(Qt::white);
            color.setAlpha(162);
            return color;
        }
    }
    else if(role == ValidateInfo) 
    {
        auto langName = this->getLangName(index);
        const auto& validateInfo = item->getValidateTextInfo(langName);
        return QVariant::fromValue(validateInfo);
    }
    else if(role == OtherCtrlChar)
    {
        if(item->type == TreeNode::Other) {
            return this->setting->validateObj.controlCharList;
        }
    }

    return QVariant();
}

bool ClipDetectSettingTreeModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if(!index.isValid()) {
        return false;
    }

    TreeNode* item = getItem(index);

    if(role == ValidateInfo) 
    {
        auto oldValue = data(index, ValidateInfo);
        auto newValue = value;
        auto langName = this->getLangName(index);
        this->history->beginMacro("Change Detect Settings Values");
        this->setValidateInfo(item, langName, newValue, oldValue);
        this->history->endMacro();
        return true;
    }
    else if(role == OtherCtrlChar)
    {
        if(item->type == TreeNode::Other) {
            emit dataChanged(index, index);
            return true;
        }
    }

    return QAbstractItemModel::setData(index, value, role);
}


void ClipDetectSettingTreeModel::setValidateInfo(TreeNode* item, QString langName, const QVariant& newValue, const QVariant& oldValue)
{
    if(item->isGroup) {
        history->push(new DetectTreeUndo(this, item, langName, oldValue, newValue));
    }
    else {
        if(oldValue != newValue) {
            history->push(new DetectTreeUndo(this, item, langName, oldValue, newValue));
        }
    }

    auto info = newValue.value<settings::ValidateTextInfo>();
    if(item->type == TreeNode::Batch)
    {
        item->getValidateTextInfo(langName) = info;
        auto rootItem = this->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            this->findFilesNodesByName(child.get(), item->name, matchingNodes);
            for(TreeNode* filesNode : matchingNodes) {
                filesNode->getValidateTextInfo(langName) = info;
                if(filesNode->settingData) {
                    (*filesNode->settingData)[langName] = info;
                }
            }
        }
    }
    else if(item->type == TreeNode::Files)
    {
        auto oldValue = item->getValidateTextInfo(langName);
        if(item->isGroup == false && info != oldValue) {
            item->getValidateTextInfo(langName) = info;
            if(item->settingData) {
                (*item->settingData)[langName] = info;
            }
        }
        for(auto& child : item->children) {
            setValidateInfo(child.get(), langName, newValue, QVariant::fromValue(child->getValidateTextInfo(langName)));
        }
    }

}

void ClipDetectSettingTreeModel::setControlChars(const QVariant& tagStrList)
{
    auto oldValue = data(index(0, 0), OtherCtrlChar).toStringList();
    auto newValue = tagStrList.toStringList();
    if(oldValue != newValue) {
        history->push(new DetectTreeUndo(this, rootItem.get(), "", oldValue, newValue));
    }
}

Qt::ItemFlags ClipDetectSettingTreeModel::flags(const QModelIndex& index) const
{
    if(auto item = this->getItem(index)) {
        if(item->isGroup && item->type == TreeNode::Batch) {
            return Qt::ItemIsSelectable | Qt::ItemIsEnabled;
        }
    }
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
    this->mapInfosCsv = readCsv(setting->analyzeDirectoryPath() + "/MapInfos.csv");

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
    batchTopItem->name          = tr("Batch assignment");
    batchTopItem->type          = TreeNode::Batch;
    batchTopItem->validateLangMap  = langMap;
    batchTopItem->isGroup       = true;
    batchTopItem->parent        = rootItem.get();
    rootItem->children.emplace_back(std::move(batchTopItem));
    auto batchTopItemPtr = rootItem->children.back().get();

    //tuple : isFirst, isSame
    std::unordered_map<QString, std::tuple<bool, bool, TreeNode*>> checkSameParameter;

    // Batch内にnameTypeListのツリーを作成
    for(auto& nameType : batchNameTypeList)
    {
        auto batchItem  = std::make_unique<TreeNode>();
        batchItem->name             = nameType;
        batchItem->type             = TreeNode::Batch;
        batchItem->validateLangMap  = langMap;
        batchItem->parent           = batchTopItemPtr;
        batchTopItemPtr->children.emplace_back(std::move(batchItem));

        checkSameParameter.emplace(nameType, std::forward_as_tuple(true, true, batchTopItemPtr->children.back().get()));
    }

    // Filesという親アイテムを作成
    auto filesTopItem = std::make_unique<TreeNode>();
    filesTopItem->name          = tr("Specify by file");
    filesTopItem->type          = TreeNode::Files;
    filesTopItem->validateLangMap  = langMap;
    filesTopItem->isGroup       = true;
    filesTopItem->parent        = rootItem.get();
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
                mapsTopItem->name           = "Maps";
                mapsTopItem->type           = TreeNode::Files;
                mapsTopItem->validateLangMap   = langMap;
                mapsTopItem->isGroup        = true;
                mapsTopItem->parent         = filesTopItemPtr;
                filesTopItemPtr->children.emplace_back(std::move(mapsTopItem));
                mapsTopItemPtr = filesTopItemPtr->children.back().get();
            }
            auto mapItem = std::make_unique<TreeNode>();
            mapItem->name           = fileName;
            mapItem->type           = TreeNode::Files;
            mapItem->validateLangMap   = langMap;
            mapItem->isGroup        = true;
            mapItem->parent         = mapsTopItemPtr;
            mapsTopItemPtr->children.emplace_back(std::move(mapItem));
            topItemPtr = mapsTopItemPtr->children.back().get();
        }
        else {
            auto topItem = std::make_unique<TreeNode>();
            topItem->name       = fileName;
            topItem->type       = TreeNode::Files;
            topItem->validateLangMap = langMap;
            topItem->isGroup    = true;
            topItem->parent     = filesTopItemPtr;
            filesTopItemPtr->children.emplace_back(std::move(topItem));
            topItemPtr = filesTopItemPtr->children.back().get();
        }

        auto& validateInfo = this->setting->getValidationCsvDataRef(fileName);
        const auto& nameTypeList = csvNameTypeMap.at(fileName);
        for(auto& nameType : nameTypeList)
        {
            auto childItem = std::make_unique<TreeNode>();
            childItem->name = nameType;
            childItem->type = TreeNode::Files;
            if(validateInfo.find(nameType) == validateInfo.end()) {
                validateInfo[nameType] = langMap;
            }
            childItem->validateLangMap = validateInfo.at(nameType);

            auto& [isFirst, isSame, node] = checkSameParameter[nameType];
            if(isSame)
            {
                if(isFirst) {
                    node->validateLangMap = childItem->validateLangMap;
                    isFirst = false;
                }
                else {
                    for(const auto& [lang, info] : node->validateLangMap)
                    {
                        if(childItem->validateLangMap[lang] != info) {
                            isSame = false;
                            childItem->validateLangMap[lang] = settings::ValidateTextInfo{};
                        }
                    }
                }
            }

            childItem->settingData = &validateInfo[nameType];
            childItem->parent = topItemPtr;
            topItemPtr->children.emplace_back(std::move(childItem));
        }
    }

    {
        // Batchという親アイテムを作成
        auto otherSettingTopItem = std::make_unique<TreeNode>();
        otherSettingTopItem->name = tr("Other Settings");
        otherSettingTopItem->type = TreeNode::Other;
        otherSettingTopItem->validateLangMap = langMap;
        otherSettingTopItem->isGroup = true;
        otherSettingTopItem->parent = rootItem.get();
        rootItem->children.emplace_back(std::move(otherSettingTopItem));
        auto* topNode = rootItem->children.back().get();


        auto extendCtrlCharacters = std::make_unique<TreeNode>();
        extendCtrlCharacters->name = tr("control character");
        extendCtrlCharacters->type = TreeNode::Other;
        extendCtrlCharacters->isGroup = false;
        extendCtrlCharacters->parent = topNode;
        topNode->children.emplace_back(std::move(extendCtrlCharacters));
    }

    this->endResetModel();
}

void ClipDetectSettingTreeModel::SetMode(ClipDetectSettingTreeModel::TreeNode* node, const QModelIndex& index, settings::ValidateTextMode value, QUndoStack* history)
{
    if(node == nullptr) { return; }


    auto langName = this->getLangName(index);
    if(node->type == TreeNode::Batch)
    {
        node->getValidateTextInfo(langName).mode = value;

        auto rootItem = this->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            this->findFilesNodesByName(child.get(), node->name, matchingNodes);
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
        if(node->isGroup == false && value != oldValue) {
            node->getValidateTextInfo(langName).mode = value;
            if(node->settingData) {
                (*node->settingData)[langName].mode = value;
            }
        }
        for(auto& child : node->children) {
            SetMode(child.get(), index, value, history);
        }
    }

}

void ClipDetectSettingTreeModel::SetCount(ClipDetectSettingTreeModel::TreeNode* node, const QModelIndex& index, int value, QUndoStack* history)
{
    if(node == nullptr) { return; }

    auto langName = this->getLangName(index);
    if(node->type == TreeNode::Batch)
    {
        node->getValidateTextInfo(langName).count = value;
        auto rootItem = this->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            this->findFilesNodesByName(child.get(), node->name, matchingNodes);
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
        if(node->isGroup == false && value != oldValue) {
            node->getValidateTextInfo(langName).count = value;
            if(node->settingData) {
                (*node->settingData)[langName].count = value;
            }
        }
        for(auto& child : node->children) {
            SetCount(child.get(), index, value, history);
        }
    }
}

void ClipDetectSettingTreeModel::SetWidth(ClipDetectSettingTreeModel::TreeNode* node, const QModelIndex& index, int value, QUndoStack* history)
{
    if(node == nullptr) { return; }

    auto langName = this->getLangName(index);
    if(node->type == TreeNode::Batch)
    {
        node->getValidateTextInfo(langName).width = value;
        auto rootItem = this->getRootItem();
        for(const auto& child : rootItem->children)
        {
            if(child->type != TreeNode::Files) { continue; }

            std::vector<TreeNode*> matchingNodes;
            this->findFilesNodesByName(child.get(), node->name, matchingNodes);
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
        if(node->isGroup == false && value != oldValue) {
            node->getValidateTextInfo(langName).width = value;
            if(node->settingData) {
                (*node->settingData)[langName].width = value;
            }
        }
        for(auto& child : node->children) {
            SetWidth(child.get(), index, value, history);
        }
    }
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
        result.emplace_back(current);
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

void ClipDetectSettingTreeModel::DetectTreeUndo::undo()
{
    auto value = oldValue.value<settings::ValidateTextInfo>();
    this->setValue(value);
}

void ClipDetectSettingTreeModel::DetectTreeUndo::redo()
{
    auto value = newValue.value<settings::ValidateTextInfo>();
    this->setValue(value);
}

void ClipDetectSettingTreeModel::DetectTreeUndo::setValue(settings::ValidateTextInfo value)
{
    auto oldValue = item->getValidateTextInfo(langName);
    if(item->isGroup == false && value != oldValue) {
        item->getValidateTextInfo(langName) = value;
        if(item->settingData) {
            (*item->settingData)[langName] = value;
        }
    }
}

settings::ValidateTextInfo& ClipDetectSettingTreeModel::TreeNode::getValidateTextInfo(QString langName)
{
    //有効な言語追加時の追従が大変なのでここで追加もする(良くはない)
    if(this->validateLangMap.find(langName) == this->validateLangMap.end()) {
        this->validateLangMap[langName] = {};
    }
    return this->validateLangMap.at(langName);
}



ClipDetectSettingTree::ClipDetectSettingTree(std::shared_ptr<settings> setting, QUndoStack* history, QWidget* parent)
    : QTreeView(parent)
    , setting(setting)
    , model(new ClipDetectSettingTreeModel(setting, history, this))
{
    this->setModel(model);

    this->setAlternatingRowColors(true);
    this->header()->setStretchLastSection(true);
    this->header()->setSectionResizeMode(QHeaderView::Interactive);
    
    this->setAnimated(true);
    //行の高さを均一にしない
    this->setUniformRowHeights(false);

    ClipDetectSettingTreeDelegate* delegate = new ClipDetectSettingTreeDelegate(model, this);
    this->setItemDelegateForColumn(1, delegate);
    this->setItemDelegateForColumn(2, delegate);

    connect(history, &QUndoStack::indexChanged, this, [this](int) { this->update(); });
}

void ClipDetectSettingTree::setup()
{
    this->model->setupClipDetectTree();

    this->setColumnWidth(0, 200);
    auto numColumn = this->model->columnCount();
    for(int i = 1; i < numColumn; ++i) {
        this->setColumnWidth(i, 200);
    }
}


void ClipDetectSettingTree::mousePressEvent(QMouseEvent* event)
{
    QModelIndex index = indexAt(event->pos());

    if(index.isValid() == false) {
        QTreeView::mousePressEvent(event);
        return;
    }

    // 前回クリックしたセルと異なる場合、最初のクリックでは確定のみ行う
    if(index != lastClickedIndex) 
    {
        // エディターが存在している場合、値を確定して閉じる。
        if(auto* w = indexWidget(lastClickedIndex)) {
            commitData(w);
            closeEditor(w, QAbstractItemDelegate::SubmitModelCache);
            lastClickedIndex = index;
            return; // このクリックでは新しいエディタを開かない。
        }
    }

    // 2回目のクリック（または同じセルのクリック）でエディタを開く
    lastClickedIndex = index;
    edit(index);
    QTreeView::mousePressEvent(event);

}

TagWidget::TagWidget(std::shared_ptr<settings> setting, QUndoStack* history, QWidget* parent)
    : QWidget(parent), setting(std::move(setting)), history(history)
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(1, 1, 1, 1);
    mainLayout->setSpacing(0);

    QHBoxLayout* inputLayout = new QHBoxLayout(this);
    QLineEdit* input = new QLineEdit(this);
    input->setPlaceholderText("Enter a tag and press Enter");
    inputLayout->addWidget(input);

    QPushButton* addButton = new QPushButton("+", this);
    addButton->setFixedSize(20, 20);
    inputLayout->addWidget(addButton);

    mainLayout->addLayout(inputLayout); 

    tagList = new QListWidget(this);
    tagList->setViewMode(QListView::IconMode);
    mainLayout->addWidget(tagList); 

    const auto AddTag = [this, input]()
    {
        auto tagList = input->text().split(' ');
        if(tagList.empty()) {
            input->clear();
            return;
        }
        if(std::all_of(tagList.begin(), tagList.end(), [](const auto& x) { return x.isEmpty(); })) {
            input->clear();
            return;
        }

        this->history->beginMacro("Change Control Characters");
        for(auto& text : tagList) {
            if(text.isEmpty() == false) {
                addTag(text);
            }
        }
        this->history->endMacro();
        input->clear();
    };

    connect(addButton, &QPushButton::clicked, this, AddTag);

    connect(input, &QLineEdit::returnPressed, this, [this, input, AddTag]() {
        if(input->text().isEmpty()) {
            if(auto* dialog = qobject_cast<QDialog*>(this->parentWidget())) {
                dialog->close();
            }
        }
        else {
            AddTag();
        }
    });

    this->setLayout(mainLayout);

    input->setFocus();
}

void TagWidget::setup(QStringList tagTextList)
{
    for(const auto& text : tagTextList) {
        this->createTagItem(text);
    }
}

void TagWidget::addTag(const QString& text)
{
    if(this->createTagItem(text) == false) { return; }

    this->history->push(new CtrlCharUndo(this->setting, "", text));
}

bool TagWidget::createTagItem(const QString& text)
{
    if(tagStrList.contains(text)) { return false; }

    tagStrList.emplace_back(text);

    QListWidgetItem* item = new QListWidgetItem(tagList);
    QWidget* itemWidget = new QWidget(tagList);
    QHBoxLayout* layout = new QHBoxLayout(itemWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(1);

    auto* removeButton = new QToolButton(itemWidget);
    removeButton->setText("x");
    removeButton->setFixedSize(20, 20);
    layout->addWidget(removeButton);

    auto* label = new QLineEdit(text, itemWidget);
    layout->addWidget(label);

    connect(label, &QLineEdit::editingFinished, this, [this, item, label]() {
        auto text = label->text();
        auto index = tagList->row(item);
        this->history->push(new CtrlCharUndo(this->setting, tagStrList[index], text));
        tagStrList[index] = text;
    });

    //フォントからtextの幅を取得してitemのサイズを設定する
    QFontMetrics fm(label->font());

    int width = fm.horizontalAdvance(text);
    item->setSizeHint(QSize(width + 28, 20));

    itemWidget->setLayout(layout);
    tagList->setItemWidget(item, itemWidget);

    connect(removeButton, &QToolButton::clicked, this, [this, item]() {
        removeTag(item);
    });

    return true;
}

void TagWidget::removeTag(QListWidgetItem* item)
{
    auto index = tagList->row(item);
    this->history->push(new CtrlCharUndo(this->setting, tagStrList[index], ""));
    tagStrList.removeAt(index);
    delete item;
}

void TagWidget::CtrlCharUndo::undo()
{
    auto& charList = this->setting->validateObj.controlCharList;
    auto index = charList.indexOf(newValue.toString());
    if(index != -1) {
        if(oldValue.toString().isEmpty()) {
            charList.removeAt(index);
        }
        else {
            charList[index] = oldValue.toString();
        }
    }
}

void TagWidget::CtrlCharUndo::redo()
{
    auto& charList = this->setting->validateObj.controlCharList;
    if(oldValue.toString().isEmpty()) {
        if(charList.contains(newValue.toString()) == false) {
            charList.append(newValue.toString());
        }
    }
    else {
        auto index = charList.indexOf(oldValue.toString());

        auto newStr = newValue.toString();
        if(newStr.isEmpty() == false) {
            if(index != -1) {
                charList[index] = newValue.toString();
            }
        }
        else {
            auto index = charList.indexOf(oldValue.toString());
            charList.removeAt(index);
        }
    }
}
