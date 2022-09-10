#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "LanguageSelectComponent.h"
#include "WriteDialog.h"
#include "src/invoker.h"
#include "../utility.hpp"
#include "../graphics.hpp"
#include "../csv.hpp"

#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QCheckBox>
#include <QLocale>
#include <QPlainTextEdit>
#include <QDebug>
#include <QActionGroup>
#include <QScrollBar>
#include <QMimeData>
#include <QGraphicsBlurEffect>

#include <functional>
#include <unordered_map>

using namespace langscore;

enum TreeItemType{
    Main,
    Script,
    Pictures
};

enum TreeColIndex{
    CheckBox = 0,
    Name
};

enum ScriptTableCol : int {
    EnableCheck = 0,
    ScriptName,
    TextPoint,
    Original,
    NumCols
};

static std::unordered_map<Qt::CheckState, QColor> tableTextColorForState = {
    {Qt::Checked,           QColor("#fefefe")},
    {Qt::PartiallyChecked,  QColor("#9a9a9a")},
    {Qt::Unchecked,         QColor("#5a5a5a")},
};

namespace {


QString GetScriptExtension(settings::ProjectType type)
{
    auto scriptExt = ".rb";
    if(type == settings::MV || type == settings::MZ){
        scriptExt = ".js";
    }
    return scriptExt;
}

QTreeWidgetItem* getTopLevelItem(QTreeWidgetItem* item){
    while(item){
        if(item->parent() == nullptr){ return item; }
        item = item->parent();
    }
    return nullptr;
}

QString getFileName(QTreeWidgetItem* item){
    assert(1 < item->columnCount());
    return item->data(TreeColIndex::Name, Qt::UserRole).toString();
}
QString getFileName(QTableWidgetItem* item){
    assert(item->column() == ScriptTableCol::ScriptName);
    return item->data(Qt::UserRole).toString();
}

QString getScriptName(QTreeWidgetItem* item){
    assert(1 < item->columnCount());
    return item->text(TreeColIndex::Name);
}
QString getScriptName(QTableWidgetItem* item){
    assert(item->column() == ScriptTableCol::ScriptName);
    return item->text();
}

} //namespace {

WriteModeComponent::WriteModeComponent(ComponentBase* setting, QWidget* parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::WriteModeComponent)
    , scene(new QGraphicsScene(this))
    , languageButtons()
    , currentScriptWordCount(0)
    , showAllScriptContents(true)
    , _suspendHistory(false)
    , _invoker(new invoker(this))
{

    ui->setupUi(this);
    this->ui->modeDesc->hide();
    this->setAcceptDrops(true);

    //無効機能
    this->ui->updateButton->setHidden(true);

    {
        auto icon = this->ui->scriptFilterButton->icon();
        auto image = icon.pixmap(this->ui->scriptFilterButton->size()).toImage();
        graphics::ReverceHSVValue(image);
        this->ui->scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));
    }
    connect(this->ui->toPackingMode, &QToolButton::clicked, this, &WriteModeComponent::toNextPage);

    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &WriteModeComponent::treeItemSelected);
    connect(this->ui->treeWidget, &QTreeWidget::itemChanged, this, &WriteModeComponent::treeItemChanged);
    connect(this->ui->tableWidget_script, &QTableWidget::itemSelectionChanged, this, &WriteModeComponent::scriptTableSelected);
    connect(this->ui->tableWidget_script, &QTableWidget::itemChanged, this, &WriteModeComponent::scriptTableItemChanged);
    connect(this->ui->writeButton, &QPushButton::clicked, this, &WriteModeComponent::exportTranslateFiles);

    connect(this->ui->scriptFilterButton, &QToolButton::clicked, this->ui->scriptFilterButton, &QToolButton::showMenu);

    //全ての内容を表示
    auto showAllContents = new QAction(tr("Show All Contents"));
    this->ui->scriptFilterButton->addAction(showAllContents);
    showAllContents->setCheckable(true);
    showAllContents->setChecked(true);
    //無視する内容を非表示
    auto hideIgnoreContents = new QAction(tr("Hide Ignore Contents"));
    this->ui->scriptFilterButton->addAction(hideIgnoreContents);
    hideIgnoreContents->setCheckable(true);
    hideIgnoreContents->setChecked(false);

    auto actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);
    actionGroup->addAction(showAllContents);
    actionGroup->addAction(hideIgnoreContents);

    connect(showAllContents, &QAction::triggered, this, [this](){
        this->showAllScriptContents = true;
        setupScriptCsvText();
    });
    connect(hideIgnoreContents, &QAction::triggered, this, [this](){
        this->showAllScriptContents = false;
        setupScriptCsvText();
    });

    connect(this->ui->updateButton, &QPushButton::clicked, this, [this]()
    {
        auto baseMessage = tr("Update to the latest content...");
        this->dispatch(StatusMessage, {baseMessage});
        this->dispatch(SaveProject,{});
        this->_invoker->analyze(true);
        this->clear();
        this->setup();
        this->dispatch(StatusMessage, {baseMessage+tr(" Complete."), 3000});
    });

    connect(_invoker, &invoker::getStdOut, this, [this](QString text){
        text.replace("\r\n", "\n");
        this->ui->logText->insertPlainText(text);
        auto vBar = this->ui->logText->verticalScrollBar();
        vBar->setValue(vBar->maximum());
        this->update();
    });
    connect(_invoker, &invoker::update, this, [this](){ this->update(); });
    connect(_invoker, &invoker::finish, this, [this](int)
    {
        this->changeEnabledUIState(true);
        this->ui->logText->insertPlainText(tr("Done."));
    });


#ifdef QT_DEBUG
    auto* configView = new QPlainTextEdit(this);
    auto configTabIndex = this->ui->tabWidget->addTab(configView, "Config");

    connect(this->ui->tabWidget, &QTabWidget::currentChanged, this, [this, configTabIndex, configView](int index)
    {
        if(configTabIndex != index){ return; }

        QString text = this->setting->createJson();

        configView->clear();
        configView->appendPlainText(text);
    });
#endif

    this->ui->treeWidget->setStyleSheet(R"(
QTreeView::item {
    height: 28px;
})");
    this->ui->graphicsView->setStyleSheet("QGraphicsView{background : #858585;}");

}

WriteModeComponent::~WriteModeComponent(){
}

void WriteModeComponent::show()
{
    const auto& outputDirPath = this->setting->tempFileDirectoryPath();
    auto outputDir = QDir(outputDirPath);
    auto basicFiles = outputDir.entryInfoList(QStringList{"*.json"}, QDir::Files);
    for(auto& file : basicFiles){
        this->setting->writeObj.basicDataInfo.emplace_back(
            settings::BasicData{file.completeBaseName(), false, 0}
        );
    }

    auto writedScripts = [&outputDirPath]()
    {
        auto scriptCsv     = readCsv(outputDirPath + "/Scripts.csv");
        QStringList result;
        if(scriptCsv.empty()){ return result; }
        scriptCsv.erase(scriptCsv.begin()); //Headerを削除
        for(auto& line : scriptCsv){
            result.emplace_back(line[0]);
        }
        result.sort();
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }();

    for(auto& script : writedScripts)
    {
        auto [fileName, row, col] = parseScriptNameWithRowCol(script);

        auto& scriptList = this->setting->writeObj.scriptInfo;
        const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
                return x.name == fileName &&  std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
            });
            return result != scriptList.cend();
        };
        if(IsIgnoreText(fileName, row, col)){
            settings::TextPoint txtPos;
            txtPos.row = row;
            txtPos.col = col;
            auto info = settings::ScriptInfo{{fileName, false, 0}, {txtPos}};
            this->setting->writeObj.scriptInfo.emplace_back(std::move(info));
        }
    }

    this->setting->defaultLanguage = settings::getLowerBcp47Name(QLocale::system());
    for(auto& lang : this->setting->languages)
    {
        if(lang.languageName == QLocale::system().bcp47Name()){
            lang.enable = true;
        }
    }

    this->setup();
}

void WriteModeComponent::clear()
{
    this->ui->treeWidget->blockSignals(true);
    this->ui->tableWidget->blockSignals(true);
    this->ui->tableWidget_script->blockSignals(true);

    this->ui->mainFileName->setText("");
    this->ui->scriptFileName->setText("");
    this->ui->mainFileWordCount->setText("");
    this->ui->scriptFileWordCount->setText("");

    this->showAllScriptContents = true;
    this->ui->treeWidget->clear();
    this->ui->tableWidget->clear();
    this->ui->tableWidget_script->clear();
    this->ui->scriptViewer->clear();
    for(auto langButton : this->languageButtons){
        this->ui->langTabGrid->removeWidget(langButton);
    }
    this->languageButtons.clear();

    this->ui->treeWidget->blockSignals(false);
    this->ui->tableWidget->blockSignals(false);
    this->ui->tableWidget_script->blockSignals(false);

    this->update();
}

void WriteModeComponent::treeItemSelected()
{
    auto selectedItems = this->ui->treeWidget->selectedItems();
    if(selectedItems.empty()){ return; }
    auto item = selectedItems[0];
    auto topLevelItem = getTopLevelItem(item);
    if(topLevelItem == nullptr){ return; }

    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
    if(treeType == TreeItemType::Main){
        auto fileName = item->data(TreeColIndex::Name, Qt::UserRole).toString();
        this->ui->tabWidget->setCurrentIndex(1);
        showNormalCsvText(item->text(TreeColIndex::Name), fileName);
    }
    else if(treeType == TreeItemType::Script)
    {
        auto changedItemScriptFileName = ::getFileName(item);
        this->ui->tabWidget->setCurrentIndex(2);

        auto scriptExt = ::GetScriptExtension(this->setting->projectType);
        auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + changedItemScriptFileName + scriptExt;
        this->ui->scriptViewer->showFile(scriptFilePath);

        auto targetTable = this->ui->tableWidget_script;
        auto rows = targetTable->rowCount();
        if(rows <= 0){ return; }
        for(int i=0; i<rows; ++i){
            if(auto scriptNameItem = this->scriptTableItem(i, ScriptTableCol::ScriptName)){
                auto scriptFileName = ::getFileName(scriptNameItem);
                if(scriptFileName.contains(changedItemScriptFileName)){
                    targetTable->scrollToItem(scriptNameItem, QAbstractItemView::ScrollHint::PositionAtCenter);
                    targetTable->selectRow(i);
                    break;
                }
            }
        }
    }
    else if(treeType == TreeItemType::Pictures)
    {
        if(item == topLevelItem){ return; }
        if(item->parent() == topLevelItem){ return; }

        auto path = ::getFileName(item);
        if(path.isEmpty()){ return; }
        scene->clear();
        this->ui->graphicsView->verticalScrollBar()->setValue(0);
        this->ui->graphicsView->horizontalScrollBar()->setValue(0);
        QDir graphFolder(this->setting->tempGraphicsFileDirectoryPath());
        auto imagePath = graphFolder.absoluteFilePath(path);
        QImage image(imagePath);
        auto* imageItem = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        scene->addItem(imageItem);
        this->ui->graphicsView->setScene(scene);

        this->ui->tabWidget->setCurrentIndex(3);
    }
}

void WriteModeComponent::treeItemChanged(QTreeWidgetItem *_item, int column)
{
    auto topLevelItem = ::getTopLevelItem(_item);
    if(topLevelItem == nullptr){ return; }
    if(column != 0){ return; }

    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);

    auto selectedItems = this->ui->treeWidget->selectedItems();
    if(selectedItems.contains(_item) == false){
        selectedItems.clear();
        selectedItems.emplace_back(_item);
    }

    const auto NotifyTreeUndoCommand = [this](std::vector<QUndoCommand *> result, QString groupName){

        if(result.empty()){ return; }
        if(_suspendHistory){
            this->discardCommand(std::move(result));
            return;
        }
        const QSignalBlocker blocker(this->ui->treeWidget);
        if(result.size() == 1){
            this->history->push(result[0]);
        }
        else
        {
            this->history->beginMacro(groupName);
            for(auto* command : result){
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
        std::vector<QUndoCommand *> result;
        for(auto item : selectedItems){
            if(newCheckVal == treeCheckState){ continue; }
            result.emplace_back(new TreeUndo(this, item, newCheckVal, treeCheckState));
        }

        NotifyTreeUndoCommand(std::move(result), tr("Main Tree Change Enable State"));
    }
    else if(treeType == TreeItemType::Script)
    {
        auto fileName = ::getFileName(_item);
        const auto& scriptList = this->setting->fetchScriptInfo(fileName);
        //変更前(現在)のチェック状態を取得
        auto treeCheckState = Qt::Checked;
        if(scriptList.ignore){
            treeCheckState = Qt::Unchecked;
            newCheckVal = Qt::Checked;
        }
        else if(scriptList.ignorePoint.empty() == false){
            treeCheckState = Qt::PartiallyChecked;
            newCheckVal = Qt::Unchecked;
        }

        if(newCheckVal == Qt::Checked){
            //チェックを付ける場合は、テーブルの選択状態によってツリーのチェック状態を変更。
            auto scriptName = ::getScriptName(_item);
            newCheckVal = getTreeCheckStateBasedOnTable(scriptName);
        }
        std::vector<QUndoCommand *> result;
        for(auto item : selectedItems){
            if(newCheckVal == treeCheckState){ continue; }
            result.emplace_back(new TreeUndo(this, item, newCheckVal, treeCheckState));
        }

        NotifyTreeUndoCommand(std::move(result), tr("Script Tree Change Enable State"));
    }
    else if(treeType == TreeItemType::Pictures)
    {
        std::vector<QTreeWidgetItem*> checkItems;
        for(auto item : selectedItems)
        {
            //画像アイテムの親が選択されていたら、そちらを優先する。
            if(item->parent() == topLevelItem){
                checkItems.emplace_back(item);
            }
        }

        for(auto item : selectedItems)
        {
            if(std::find(checkItems.cbegin(), checkItems.cend(), item) != checkItems.cend()){
                continue;
            }
            if(std::find(checkItems.cbegin(), checkItems.cend(), item->parent()) != checkItems.cend()){
                continue;
            }
            checkItems.emplace_back(item);
        }

        this->ui->treeWidget->blockSignals(true);
        std::vector<QUndoCommand *> result;
        for(auto* item : checkItems)
        {
            result.emplace_back(new TreeUndo(this, item, newCheckVal, item->checkState(0)));
            //フォルダーのチェックを変更
            if(item->parent() == topLevelItem)
            {
                //親が選択されていた場合、親優先でチェック状態を変更する。
                //この時、子のチェック状態も合わせて変更する。
                auto numChilds = item->childCount();
                for(int i=0; i<numChilds; ++i){
                    auto childItem = item->child(i);
                    result.emplace_back(new TreeUndo(this, childItem, newCheckVal, childItem->checkState(0)));
                }
            }
            else
            {
                //同階層のオブジェクトチェック
                auto folderItem = item->parent();
                const auto numChilds = folderItem->childCount();
                int numChecked = 0;
                for(int i=0; i<numChilds; ++i){
                    auto childItem = folderItem->child(i);
                    if(childItem->checkState(0) == Qt::Checked){
                        numChecked++;
                    }
                }
                auto parentFolderCheckState = Qt::Checked;
                if(numChecked == 0){
                    parentFolderCheckState = Qt::Unchecked;
                }
                else if(numChecked < numChilds){
                    parentFolderCheckState = Qt::PartiallyChecked;
                }
                folderItem->setCheckState(0, parentFolderCheckState);
            }
        }
        this->ui->treeWidget->blockSignals(false);

        NotifyTreeUndoCommand(std::move(result), tr("Graphics Tree Change Enable State"));

    }
}

void WriteModeComponent::scriptTableSelected()
{
    auto selectItems = this->ui->tableWidget_script->selectedItems();
    if(selectItems.isEmpty()){ return; }
    auto item = selectItems[0];
    int row = item->row();

    auto scriptNameItem = this->scriptTableItem(row, ScriptTableCol::ScriptName);
    auto textPointItem = this->scriptTableItem(row, ScriptTableCol::TextPoint);
    if(scriptNameItem == nullptr || textPointItem == nullptr){ return; }
    auto fileName = ::getFileName(scriptNameItem);

    auto [textRow,textCol] = ::parseScriptWithRowCol(textPointItem->text());
    auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + fileName + GetScriptExtension(this->setting->projectType);
    this->ui->scriptViewer->showFile(scriptFilePath);

    auto scriptName = ::getScriptName(scriptNameItem);
    this->ui->scriptFileName->setText(scriptName + "(" + fileName + ")");

    //スクリプトのスクロール
    int textLen = 0;
    if(auto i = this->scriptTableItem(row, ScriptTableCol::Original)){
        auto text = i->text();
        textLen = text.length();
    }
    this->ui->scriptViewer->scrollWithHighlight(textRow, textCol, textLen);
}

void WriteModeComponent::scriptTableItemChanged(QTableWidgetItem *item)
{
    if(item->column() != 0){ return; }

    auto selectItems = this->ui->tableWidget_script->selectedItems();
    //選択アイテムでは無いアイテムをクリックした場合、クリック側のみを処理する。
    QSet<int> rows;
    for(auto* _item : selectItems){ rows.insert(_item->row()); }
    if(rows.contains(item->row()) == false){
        rows.clear();
        rows.insert(item->row());
    }

    //無視リストの操作
    auto check = item->checkState();
    bool ignore = check == Qt::Unchecked;
    std::vector<QUndoCommand *> result;
    for(int row : rows)
    {
        if(auto checkItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck)){
            result.emplace_back(new TableUndo(this, checkItem, check, ignore ? Qt::Checked : Qt::Unchecked));
        }
    }

    if(result.empty()){ return; }
    if(_suspendHistory){
        discardCommand(std::move(result));
        return;
    }

    const QSignalBlocker blocker(this->ui->tableWidget_script);
    if(result.size() == 1){
        this->history->push(result[0]);
    }
    else
    {
        this->history->beginMacro(tr("Change Script Table Check"));
        for(auto* command : result){
            this->history->push(command);
        }
        this->history->endMacro();
    }
}

void WriteModeComponent::exportTranslateFiles()
{
    if(this->setting->writeObj.exportDirectory.isEmpty()){
        this->setting->writeObj.exportDirectory = this->setting->translateDirectoryPath();
    }
    this->setGraphicsEffect(new QGraphicsBlurEffect);
    this->changeEnabledUIState(false);

    WriteDialog dialog(this->setting, this);

    auto execCode = dialog.exec();
    this->setGraphicsEffect(nullptr);
    if(execCode == QDialog::Rejected){ return; }

    QDir lsProjDir(this->setting->langscoreProjectDirectory);
    auto relativePath = lsProjDir.relativeFilePath(dialog.outputPath());
    this->setting->writeObj.exportDirectory = relativePath;
    this->setting->writeObj.exportByLanguage = dialog.writeByLanguage();

    if(dialog.backup()){ backup(); }

    this->ui->tabWidget->setCurrentWidget(this->ui->tab_4);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));

    this->dispatch(SaveProject,{});
    _invoker->write();
}

void WriteModeComponent::setup()
{
    this->ui->tabWidget->setCurrentIndex(0);
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->treeWidget->blockSignals(true);

    //プロジェクトパスの表示
    this->ui->projectPath->setText(this->setting->gameProjectPath);
    this->ui->lsProjPath->setText(this->setting->langscoreProjectDirectory);


    //フォントの設定
    auto fontExtensions = QStringList() << "*.ttf" << "*.otf";
    {
        QFileInfoList files = QDir(qApp->applicationDirPath()+"/resources/fonts").entryInfoList(fontExtensions, QDir::Files);
        for(auto& info : files){
            auto path = info.absoluteFilePath();
            this->setting->fontIndexList.emplace_back(settings::Global, QFontDatabase::addApplicationFont(path), info.fileName());
        }
    }
    //プロジェクトに登録されたフォント
    {
        QFileInfoList files = QDir(this->setting->langscoreProjectDirectory+"/Fonts").entryInfoList(fontExtensions, QDir::Files);
        for(auto& info : files){
            auto path = info.absoluteFilePath();
            this->setting->fontIndexList.emplace_back(settings::Local, QFontDatabase::addApplicationFont(path), info.fileName());
        }
    }

    //Languages
    const std::vector<QLocale> languages = {
        {QLocale::English},
        {QLocale::Spanish},
        {QLocale::German},
        {QLocale::French},
        {QLocale::Italian},
        {QLocale::Japanese},
        {QLocale::Korean},
        {QLocale::Russian},
        {QLocale::Chinese, QLocale::SimplifiedChineseScript},
        {QLocale::Chinese, QLocale::TraditionalChineseScript}
    };

    int count = 0;
    auto* buttonGroup = new QButtonGroup(this);
    for(const auto& lang : languages)
    {
        auto langComp = new LanguageSelectComponent(lang, this, this);
        langComp->attachButtonGroup(buttonGroup);
        this->ui->langTabGrid->addWidget(langComp, count/3, count%3);
        langComp->update();
        languageButtons.emplace_back(langComp);
        count++;
    }

    setupTree();
    setupScriptCsvText();

    this->setting->saveForProject();

    this->ui->tableWidget_script->blockSignals(false);
    this->ui->treeWidget->blockSignals(false);

    this->update();
}

void WriteModeComponent::setupTree()
{
    const auto topLevelItemBGColor  = QColor(128, 128, 128, 90);
    const auto graphicFolderBGColor = QColor(128, 128, 128, 90);
    const auto translateFolder = this->setting->tempFileDirectoryPath();
    //Main
    {
        auto mapInfosCsv = readCsv(translateFolder + "/MapInfos.csv");
        QDir sourceDir(this->setting->tempFileDirectoryPath());
        auto files = sourceDir.entryInfoList();

        auto mainItem = new QTreeWidgetItem();
        mainItem->setText(0, "Main");
        mainItem->setData(0, Qt::UserRole, TreeItemType::Main);
        mainItem->setBackground(0, topLevelItemBGColor);
        mainItem->setBackground(1, topLevelItemBGColor);
        mainItem->setForeground(0, Qt::white);
        this->ui->treeWidget->addTopLevelItem(mainItem);
        for(const auto& file : files)
        {
            if(file.suffix() != "json"){ continue; }

            auto fileViewName = file.completeBaseName();
            if(fileViewName == "config"){ continue; }
            else if(fileViewName == "Scripts"){ continue; }
            else if(fileViewName == "Graphics"){ continue; }

            if(QFile::exists(translateFolder + "/" + fileViewName + ".csv") == false){
                continue;
            }
            int mapID = -1;
            if(QRegularExpression("Map\\d\\d\\d").match(fileViewName).hasMatch())
            {
                auto fileID = fileViewName;
                fileID.remove("Map");
                bool isOK = false;
                mapID = fileID.toInt(&isOK);
                if(isOK == false){
                    mapID = -1;
                }
                else{
                    auto result = std::find_if(mapInfosCsv.cbegin(), mapInfosCsv.cend(), [mapID](const auto& x){
                        return x[0].toInt() == mapID;
                    });
                    if(result != mapInfosCsv.cend()){
                        fileViewName += "("+ (*result)[2] +")";
                    }
                }
            }
            auto child = new QTreeWidgetItem();
            child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
            child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
            child->setText(TreeColIndex::Name, fileViewName);
            child->setData(TreeColIndex::Name, Qt::UserRole, file.fileName());

            const auto& basicDataInfo = this->setting->writeObj.basicDataInfo;
            auto result = std::find_if(basicDataInfo.cbegin(), basicDataInfo.cend(), [fileName = file.fileName()](const auto& x){
                return x.name.contains(fileName);
            });
            if(result != basicDataInfo.cend()){
                child->setCheckState(TreeColIndex::CheckBox, result->ignore ? Qt::Unchecked : Qt::Checked);
            }

            mainItem->addChild(child);
        }
    }

    //Script
    {
        scriptFileNameMap.clear();
        auto scriptExt = ::GetScriptExtension(this->setting->projectType);
        auto scriptCsv     = readCsv(translateFolder + "/Scripts.csv");
        auto writedScripts = [&scriptCsv, &scriptExt]()
        {
            QStringList result;
            if(scriptCsv.empty()){ return result; }
            scriptCsv.erase(scriptCsv.begin());
            for(auto& line : scriptCsv){
                auto& file = line[0];
                auto index = file.indexOf(scriptExt);
                file.remove(index, file.size()-index);
                result.emplace_back(std::move(file));
            }
            result.sort();
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        }();

        auto scriptItem    = new QTreeWidgetItem();
        scriptItem->setText(0, "Script");
        scriptItem->setData(0, Qt::UserRole, TreeItemType::Script);
        scriptItem->setBackground(0, topLevelItemBGColor);
        scriptItem->setBackground(1, topLevelItemBGColor);
        scriptItem->setForeground(0, Qt::white);
        this->ui->treeWidget->addTopLevelItem(scriptItem);
        auto scriptFolder  = this->setting->tempScriptFileDirectoryPath();
        auto scriptFiles   = readCsv(scriptFolder + "/_list.csv");
        auto extWithoutDot = scriptExt;
        extWithoutDot.remove(0,1);
        for(const auto& l : scriptFiles)
        {
            const auto& scriptFileName = l[0];
            const auto& scriptName = l[1];
            scriptFileNameMap.emplace_back(scriptFileName, scriptName);
            auto file = QFileInfo(scriptFolder+"/"+scriptFileName+scriptExt);
            if(file.suffix() != extWithoutDot){ continue; }
            if(file.size() == 0){ continue; }

            if(std::find_if(writedScripts.cbegin(), writedScripts.cend(), [&scriptFileName](const auto& x){
                return x.contains(scriptFileName);
            }) == writedScripts.cend()){
                continue;
            }
            auto child = new QTreeWidgetItem();
            child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
            //チェックを外すとこのスクリプトを翻訳から除外します。
            child->setToolTip(TreeColIndex::CheckBox, tr("Unchecking the box excludes this script from translation."));
            child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
            child->setText(TreeColIndex::Name, scriptName);
            child->setData(TreeColIndex::Name, Qt::UserRole, withoutExtension(file.fileName()));

            const auto& scriptList = this->setting->writeObj.scriptInfo;
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&scriptFileName](const auto& script){
                return script.name.contains(scriptFileName);
            });
            if(result != scriptList.cend())
            {
                if(result->ignore){
                    child->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked);
                }
                else if(result->ignorePoint.empty() == false){
                    child->setCheckState(TreeColIndex::CheckBox, Qt::PartiallyChecked);
                }
                else{
                    child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
                }
            }

            scriptItem->addChild(child);
        }
    }

    //Picture
    {
        auto graphicsRootItem    = new QTreeWidgetItem();
        graphicsRootItem->setText(0, "Graphics");
        graphicsRootItem->setData(0, Qt::UserRole, TreeItemType::Pictures);
        graphicsRootItem->setBackground(0, topLevelItemBGColor);
        graphicsRootItem->setBackground(1, topLevelItemBGColor);
        graphicsRootItem->setForeground(0, Qt::white);
        this->ui->treeWidget->addTopLevelItem(graphicsRootItem);

        QDir graphicsFolder(this->setting->tempGraphicsFileDirectoryPath());
        QFileInfoList graphFolders = graphicsFolder.entryInfoList(QDir::AllDirs|QDir::NoDotAndDotDot);

        for(const auto& graphFolder : graphFolders)
        {
            auto folderRoot = new QTreeWidgetItem();
            folderRoot->setText(TreeColIndex::Name, graphFolder.baseName());
            folderRoot->setBackground(TreeColIndex::CheckBox, graphicFolderBGColor);
            folderRoot->setBackground(TreeColIndex::Name, graphicFolderBGColor);
            folderRoot->setForeground(TreeColIndex::Name, Qt::white);
            QDir childFolder(graphFolder.absoluteFilePath());
            QFileInfoList files = childFolder.entryInfoList(QStringList() << "*.*", QDir::Files);
            const auto numPictures = files.size();
            int ignorePictures = 0;

            for(const auto& pict : files)
            {
                if(pict.size() == 0){ continue; }

                auto pictureFileName = pict.completeBaseName();
                auto child = new QTreeWidgetItem();
                child->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);
                //チェックを外すとこのスクリプトを翻訳から除外します。
                child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
                child->setText(TreeColIndex::Name, pictureFileName);
                child->setData(TreeColIndex::Name, Qt::UserRole, graphicsFolder.relativeFilePath(pict.absoluteFilePath()));
                child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);

                auto& pixtureList = this->setting->writeObj.ignorePicturePath;
                auto result = std::find_if(pixtureList.begin(), pixtureList.end(), [&pictureFileName](const auto& pic){
                    return QFileInfo(pic).completeBaseName() == pictureFileName;
                });
                if(result != pixtureList.end()){
                    child->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked);
                    ignorePictures++;
                }
                else {
                    child->setCheckState(TreeColIndex::CheckBox, Qt::Checked);
                }

                folderRoot->addChild(child);
            }

            if(0 < numPictures)
            {
                folderRoot->setData(TreeColIndex::CheckBox, Qt::CheckStateRole, true);

                if(ignorePictures == 0){ folderRoot->setCheckState(TreeColIndex::CheckBox, Qt::Checked); }
                else if(ignorePictures < numPictures){ folderRoot->setCheckState(TreeColIndex::CheckBox, Qt::PartiallyChecked); }
                else{ folderRoot->setCheckState(TreeColIndex::CheckBox, Qt::Unchecked); }
            }
            graphicsRootItem->addChild(folderRoot);
        }
    }
}

void WriteModeComponent::setupScriptCsvText()
{
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->tableWidget_script->clear();
    const auto tempFolder = this->setting->tempFileDirectoryPath();
    auto csv = readCsv(tempFolder + "/Scripts.csv");
    if(csv.empty()){
        this->ui->tableWidget_script->blockSignals(false);
        return;
    }

    using ScriptLineInfo = std::tuple<QString, size_t, size_t>;

    const auto& scriptList = this->setting->writeObj.scriptInfo;
    const auto IsIgnoreText = [&scriptList](const ScriptLineInfo& info){
        auto [fileName, row, col] = info;
        fileName = withoutExtension(std::move(fileName));
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
            return withoutExtension(x.name) == fileName && std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
        });
        return result != scriptList.cend();
    };

    const auto IsIgnoreScript = [&scriptList](QString fileName){
        fileName = withoutExtension(std::move(fileName));
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
            return withoutExtension(x.name) == fileName && x.ignore;
        });
        return result != scriptList.cend();
    };

    const auto TextColor = [&](const ScriptLineInfo& lineInfo){
        if(IsIgnoreScript(std::get<0>(lineInfo))){ return tableTextColorForState[Qt::Unchecked]; }
        if(IsIgnoreText(lineInfo)){ return tableTextColorForState[Qt::PartiallyChecked]; }
        return tableTextColorForState[Qt::Checked];
    };

    if(showAllScriptContents == false)
    {
        auto rm_result = std::remove_if(csv.begin()+1, csv.end(), [&IsIgnoreScript, &IsIgnoreText](const std::vector<QString>& line){
            auto result = parseScriptNameWithRowCol(line[0]);
            return IsIgnoreScript(std::get<0>(result)) || IsIgnoreText(result);
        });
        csv.erase(rm_result, csv.end());
    }

    //ヘッダーを除外するので-1
    const auto numTableItems = csv.size()-1;
    QTableWidget *targetTable = this->ui->tableWidget_script;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->setRowCount(numTableItems);
    targetTable->setColumnCount(ScriptTableCol::NumCols);

    targetTable->setHorizontalHeaderLabels(QStringList() << "Include" << "ScriptName" << "TextPoint" << "Original Text");

    currentScriptWordCount = 0;
    const auto& scriptExt = GetScriptExtension(this->setting->projectType);
    for(int r=0; r<numTableItems; ++r)
    {
        const auto& line = csv[r+1];
        const auto& scriptLineInfo = line[0];
        const auto& originalText = line.size()<=1 ? "" : line[1];
        auto lineParsedResult = parseScriptNameWithRowCol(scriptLineInfo);

        const auto& textColor = TextColor(lineParsedResult);

        //チェックボックス
        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        targetTable->setItem(r, ScriptTableCol::EnableCheck, checkItem);

        const auto scriptTableItemFlags = Qt::ItemIsEnabled|Qt::ItemIsSelectable;
        //名前
        {
            auto* item = new QTableWidgetItem(originalText);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::Original, item);

            checkItem->setData(Qt::CheckStateRole, IsIgnoreText(lineParsedResult) ? Qt::Unchecked : Qt::Checked);
        }

        //チェックセル以外を初期化
        auto [fileName, row, col] = lineParsedResult;
        if(fileName.contains(scriptExt)){
            fileName.chop(scriptExt.length());
        }
        currentScriptWordCount += wordCountUTF8(originalText);

        {
            auto scriptName = getScriptName(fileName);
            auto* item = new QTableWidgetItem(scriptName);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            item->setData(Qt::UserRole, withoutExtension(fileName));
            targetTable->setItem(r, ScriptTableCol::ScriptName, item);
        }
        //テキストの位置
        {
            auto* item = new QTableWidgetItem(QString("%1:%2").arg(row).arg(col));
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);
        }
    }

    targetTable->resizeColumnsToContents();
    targetTable->update();

    this->ui->tableWidget_script->blockSignals(false);

    this->ui->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));
}


void WriteModeComponent::showNormalCsvText(QString treeItemName, QString fileName)
{
    const auto translateFolder = this->setting->tempFileDirectoryPath();
    auto csv = readCsv(translateFolder + "/" + withoutExtension(fileName) + ".csv");
    if(csv.empty()){ return; }

    this->ui->mainFileName->setText(treeItemName);

    const auto& header = csv[0];
    auto numTableItems = csv.size()-1;
    QTableWidget *targetTable = this->ui->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->setRowCount(numTableItems);
    targetTable->setColumnCount(header.size());
    for(int c=0; c<header.size(); ++c){
        auto* item = new QTableWidgetItem(header[c]);
        targetTable->setHorizontalHeaderItem(c, item);
    }

    size_t wordCount = 0;
    for(int r=0; r<numTableItems; ++r)
    {
        const auto& line = csv[r+1];
        for(int c=0; c<line.size(); ++c){
            auto text = line[c];
            wordCount += wordCountUTF8(text);
            auto* item = new QTableWidgetItem(text);
            targetTable->setItem(r, c, item);
        }
    }
    this->ui->mainFileWordCount->setText(tr("Word Count : %1").arg(wordCount));
}


void WriteModeComponent::setTreeItemCheck(QTreeWidgetItem *item, Qt::CheckState check)
{
    const bool ignore = check == Qt::Unchecked;
    auto topLevelItem = getTopLevelItem(item);
    if(topLevelItem == nullptr){ return; }
    const auto treeType = topLevelItem->data(TreeColIndex::CheckBox, Qt::UserRole);
    item->setCheckState(0, check);
    if(treeType == TreeItemType::Main){
        writeToBasicDataListSetting(item, ignore);
        this->update();
        return;
    }
    else if(treeType == TreeItemType::Pictures){
        writeToPictureListSetting(item, ignore);
        this->update();
        return;
    }

    writeToScriptListSetting(item, ignore);

    //テーブル側への反映
    this->ui->tableWidget_script->blockSignals(true);
    auto targetItemRow = fetchScriptTableSameFileRows(::getScriptName(item));
    const auto textColor = tableTextColorForState[check];

    for(auto r : targetItemRow)
    {
        this->setTableItemTextColor(r, textColor);
    }
    this->ui->tableWidget_script->blockSignals(false);

    this->update();
}

void WriteModeComponent::setScriptTableItemCheck(QTableWidgetItem* item, Qt::CheckState check)
{
    //無視リストの操作
    const int row = item->row();
    {
        const QSignalBlocker scriptTableBlocker(this->ui->tableWidget_script);
        auto baseCheckItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck);
        if(baseCheckItem)
        {
            writeToIgnoreScriptLine(row, check == Qt::Unchecked);
            baseCheckItem->setCheckState(check);
        }
    }

    auto scriptItem = this->scriptTableItem(row, ScriptTableCol::ScriptName);
    QString scriptName = scriptItem ? scriptItem->text() : "";

    //チェックしたアイテムに関連している、テーブル側のアイテムの色を変える
    auto rows     = fetchScriptTableSameFileRows(scriptName);
    auto treeItem = fetchScriptTreeSameFileItem(scriptName);

    const QSignalBlocker scriptTableBlocker(this->ui->treeWidget);
    if(treeItem && treeItem->checkState(0) == Qt::Unchecked)
    {
        for(auto row : rows){
            setTableItemTextColor(row, tableTextColorForState[Qt::Unchecked]);
        }
    }
    else
    {
        for(auto row : rows)
        {
            auto checkItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck);
            if(checkItem == nullptr){ continue; }
            setTableItemTextColor(row, tableTextColorForState[checkItem->checkState()]);
        }
    }

    //ツリー側のチェック状態を変更
    if(treeItem)
    {
        //同じスクリプトが全て無視されたかをチェック
        auto treeCheckState = getTreeCheckStateBasedOnTable(scriptName);
        //スクリプトの無視状態が最優先なので、チェック無しの場合は何もしない。
        if(treeItem->checkState(0) != Qt::Unchecked){
            treeItem->setCheckState(0, treeCheckState);
            //ヒストリーを介さない方法で変更するため、ここで設定ファイルへの書き込みを行う
            writeToScriptListSetting(treeItem, check == Qt::Unchecked);
        }
        treeItem->setForeground(1, tableTextColorForState[treeCheckState]);
    }

    this->update();
}

void WriteModeComponent::writeToBasicDataListSetting(QTreeWidgetItem *item, bool ignore)
{
    auto fileName = ::getFileName(item);
    auto& info = this->setting->fetchBasicDataInfo(fileName);
    info.ignore = ignore;
}

void WriteModeComponent::writeToScriptListSetting(QTreeWidgetItem *item, bool ignore)
{
    auto fileName = ::getFileName(item);
    auto& info = this->setting->fetchScriptInfo(fileName);
    info.ignore = ignore;
}

void WriteModeComponent::writeToIgnoreScriptLine(int row, bool ignore)
{
    auto textPosItem = this->scriptTableItem(row, ScriptTableCol::TextPoint);
    if(textPosItem == nullptr){ return; }
    auto [lineNum, col] = parseScriptWithRowCol(textPosItem->text());
    auto fileName = getScriptFileNameFromTable(row);
    if(fileName.isEmpty()){ return; }
    //MainComponentのinvokeAnalyzeと重複
    //コンフィグの内容を変更
    if(ignore)
    {
        auto& info = setting->fetchScriptInfo(fileName);
        settings::TextPoint p;
        p.row = lineNum;
        p.col = col;
        info.ignorePoint.emplace_back(std::move(p));
    }
    else{
        setting->removeScriptInfoPoint(fileName, {lineNum, col});
    }
}

void WriteModeComponent::writeToPictureListSetting(QTreeWidgetItem *item, bool ignore)
{
    auto filePath = ::getFileName(item);
    if(filePath.isEmpty()){ return; }
    QDir graphicsDir(this->setting->tempGraphicsFileDirectoryPath());
    filePath = graphicsDir.relativeFilePath(filePath);
    auto& picturePathList = this->setting->writeObj.ignorePicturePath;
    auto result = std::find_if(picturePathList.cbegin(), picturePathList.cend(), [&filePath](const auto& path){
        return path == filePath;
    });
    if(result != picturePathList.end()){
        if(ignore == false) {
            picturePathList.erase(result);
        }
    }
    else if(ignore){
        picturePathList.emplace_back(filePath);
    }
}

QTableWidgetItem *WriteModeComponent::scriptTableItem(int row, int col){
    if(this->ui->tableWidget_script->rowCount() <= row){ return nullptr; }
    return this->ui->tableWidget_script->item(row, col);
}

QString WriteModeComponent::getScriptName(QString fileName)
{
    fileName = QFileInfo(fileName).completeBaseName();
    auto result = std::find_if(scriptFileNameMap.cbegin(), scriptFileNameMap.cend(), [fileName](const auto& x){
        return x.first == fileName;
    });
    if(result == scriptFileNameMap.cend()){
        return "";
    }
    return result->second;
}

QString WriteModeComponent::getScriptFileName(QString scriptName)
{
    auto result = std::find_if(scriptFileNameMap.cbegin(), scriptFileNameMap.cend(), [scriptName](const auto& x){
        return x.second == scriptName;
    });
    if(result == scriptFileNameMap.cend()){
        return "";
    }
    return result->first;
}

QString WriteModeComponent::getScriptFileNameFromTable(int row)
{
    if(auto scriptNameItem = this->scriptTableItem(row, ScriptTableCol::ScriptName)){
        return ::getFileName(scriptNameItem);
    }
    return "";
}

void WriteModeComponent::setFontList(std::vector<QString> fontPaths)
{
    auto lsProjDir = QDir(this->setting->langscoreProjectDirectory);
    if(lsProjDir.exists("/Fonts") == false){
        lsProjDir.mkdir("Fonts");
    }
    std::vector<QFont> fonts;
    for(auto& path : fontPaths){

        QFile file(path);
        auto fontFileName = QFileInfo(path).fileName();
        auto destPath = lsProjDir.absolutePath()+"/Fonts/"+fontFileName;
        if(QFile::exists(destPath) == false){
            file.copy(destPath);
        }

        auto fontIndex = QFontDatabase::addApplicationFont(path);
        auto appFonts = QFontDatabase::applicationFontFamilies(fontIndex);
        for(const auto& appFont : appFonts){
            this->setting->fontIndexList.emplace_back(settings::Local, fontIndex, appFont);
            fonts.emplace_back(appFont);
        }
    }
    for(auto* langButton : languageButtons){
        langButton->setFontList(fonts, "");
    }

}

void WriteModeComponent::changeEnabledUIState(bool enable)
{
    this->ui->treeWidget->setEnabled(enable);
    this->ui->tableWidget_script->setEnabled(enable);
    this->ui->writeButton->setEnabled(enable);
    this->ui->updateButton->setEnabled(enable);
}

void WriteModeComponent::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();

    std::vector<QString> fontPathList;
    for(auto& url : urlList)
    {
        QFileInfo fileInfo(url.toLocalFile());
        auto ext = fileInfo.suffix();
        if(ext != "ttf" && ext != "otf"){ continue; }

        fontPathList.emplace_back(fileInfo.absoluteFilePath());
    }
    setFontList(std::move(fontPathList));
}

void WriteModeComponent::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();
    for(auto& url : urlList)
    {
        QFileInfo fileInfo(url.toLocalFile());
        auto ext = fileInfo.suffix();
        if(ext != "ttf" && ext != "otf"){ continue; }

        event->acceptProposedAction();
        return;
    }
}

std::vector<int> WriteModeComponent::fetchScriptTableSameFileRows(QString scriptName)
{
    std::vector<int> result;
    const auto tableRows = this->ui->tableWidget_script->rowCount();
    for(int i=0; i<tableRows; ++i)
    {
        if(auto scriptNameItem = this->scriptTableItem(i, ScriptTableCol::ScriptName))
        {
            auto itemText = ::getScriptName(scriptNameItem);
            if(scriptName == itemText){
                result.emplace_back(i);
            }
        }
    }

    return result;
}

QTreeWidgetItem* WriteModeComponent::fetchScriptTreeSameFileItem(QString scriptName)
{
    const auto numItems = this->ui->treeWidget->topLevelItemCount();
    for(int i=0; i<numItems; ++i)
    {
        auto parentItem = this->ui->treeWidget->topLevelItem(i);
        const auto treeType = parentItem->data(TreeColIndex::CheckBox, Qt::UserRole);
        if(treeType != TreeItemType::Script){ continue; }

        auto numChilds = parentItem->childCount();
        for(int c=0; c<numChilds; ++c)
        {
            auto childItem = parentItem->child(c);
            if(scriptName != ::getScriptName(childItem)){ continue; }
            return childItem;
        }
    }
    return nullptr;
}

void WriteModeComponent::setTableItemTextColor(int row, QBrush color)
{
    const auto tableCol = this->ui->tableWidget_script->columnCount();
    for(int c=0; c<tableCol; ++c){
        if(auto i = this->scriptTableItem(row, c)){
            i->setForeground(color);
        }
    }
}

Qt::CheckState WriteModeComponent::getTreeCheckStateBasedOnTable(QString scriptName)
{
    //同じスクリプトが全て無視されたかをチェック
    int enableCheckCount = 0;
    auto rows = fetchScriptTableSameFileRows(scriptName);
    for(auto row : rows)
    {
        if(auto checkItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck))
        {
            if(checkItem->checkState() == Qt::Checked){
                enableCheckCount++;
            }
        }
    }

    auto treeCheckState = Qt::Checked;
    if(rows.size() != enableCheckCount){
        treeCheckState = (enableCheckCount == 0) ? Qt::Unchecked : Qt::PartiallyChecked;
    }

    return treeCheckState;
}

void WriteModeComponent::updateScriptWordCount(QString text, Qt::CheckState state)
{
    auto wordCount = wordCountUTF8(text);
    currentScriptWordCount += (state == Qt::Unchecked) ? -wordCount : wordCount;
    this->ui->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));
}

void WriteModeComponent::backup()
{
    QDir backupDir(this->setting->langscoreProjectDirectory+"/backup");
    QStringList backupFiles;
    if(this->setting->projectType == settings::VXAce)
    {
        backupFiles = {
            "/Data/Scripts.rvdata2",
            "/Data/Translate"
        };
    }

    if(QDir(this->setting->langscoreProjectDirectory+"/backup").exists() == false){
        QDir(this->setting->langscoreProjectDirectory).mkdir("/backup");
    }
    QString currentDate = QDate().currentDate().toString("yyyy-MM-dd") + "-" + QTime().currentTime().toString("HHmm");
    QString backupFolderPath = this->setting->langscoreProjectDirectory+"/backup/"+currentDate;
    if(backupDir.exists() == false){
        backupDir.mkpath(backupFolderPath);
    }

    for(auto& path : backupFiles)
    {
        QFileInfo srcFileInfo(this->setting->gameProjectPath+path);
        if(srcFileInfo.isFile()){
            auto destDir = QFileInfo(backupFolderPath+path).dir();
            if(destDir.exists() == false){
                QDir().mkpath(destDir.absolutePath());
            }
            QFile(srcFileInfo.filePath()).copy(destDir.absolutePath()+"/"+srcFileInfo.fileName());
        }
        else if(srcFileInfo.exists() && srcFileInfo.isDir())
        {
            if(QDir().exists(backupFolderPath+path) == false){
                QDir().mkpath(backupFolderPath+path);
            }
            auto files = QDir(srcFileInfo.absoluteFilePath()).entryInfoList(QDir::Files);
            for(auto& file : files){
                auto destPath = backupFolderPath+path+"/"+file.fileName();
                QFile(file.absoluteFilePath()).copy(destPath);
            }
        }
    }
}

void WriteModeComponent::TableUndo::setValue(ValueType value){
    this->parent->setScriptTableItemCheck(this->target, value ? Qt::Checked : Qt::Unchecked);
}

void WriteModeComponent::TableUndo::undo(){
    this->setValue(oldValue);
    if(auto textItem = this->parent->ui->tableWidget_script->item(this->target->row(), ScriptTableCol::Original)){
        this->setText(tr("Change Table State : %1").arg(textItem->text()));
    }
    else{
        this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    }
}

void WriteModeComponent::TableUndo::redo(){
    this->setValue(newValue);
    if(auto textItem = this->parent->ui->tableWidget_script->item(this->target->row(), ScriptTableCol::Original)){
        this->setText(tr("Change Table State : %1").arg(textItem->text()));
    }
    else{
        this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    }
}

void WriteModeComponent::TreeUndo::setValue(ValueType value){
    this->parent->setTreeItemCheck(this->target, value);
}

void WriteModeComponent::TreeUndo::undo(){
    this->setValue(oldValue);
    this->setText(tr("Change Tree State : %1").arg(this->target->text(1)));
}

void WriteModeComponent::TreeUndo::redo(){
    this->setValue(newValue);
    this->setText(tr("Change Tree State : %1").arg(this->target->text(1)));
}
