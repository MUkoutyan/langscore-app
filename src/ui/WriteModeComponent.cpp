#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "MainComponent.h"
#include "LanguageSelectComponent.h"
#include "WriteDialog.h"
#include "../graphics.hpp"
#include "../invoker.h"
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

#include <functional>
#include <unordered_map>

using namespace langscore;

enum TreeItemType{
    Main,
    Script,
    Pictures
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

int wordCountUTF8(QString text)
{
    int words = 0;
    for(auto beg = text.cbegin(); beg != text.cend(); ++beg){
        if((beg->toLatin1() & 0xc0) != 0x80){ words++; }
    }
    return words;
}

QString GetScriptExtention(settings::ProjectType type)
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

}

WriteModeComponent::WriteModeComponent(ComponentBase* setting, MainComponent *parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::WriteModeComponent)
    , _parent(parent)
    , scene(new QGraphicsScene(this))
    , languageButtons()
    , currentScriptWordCount(0)
    , showAllScriptContents(true)
    , _suspendHistory(false)
{
    ui->setupUi(this);
    this->ui->modeDesc->hide();

    {
        auto icon = this->ui->scriptFilterButton->icon();
        auto image = icon.pixmap(this->ui->scriptFilterButton->size()).toImage();
        graphics::ReverceHSVValue(image);
        this->ui->scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));
    }

    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, &WriteModeComponent::treeItemSelected);
    connect(this->ui->treeWidget, &QTreeWidget::itemChanged, this, &WriteModeComponent::treeItemChanged);
    connect(this->ui->tableWidget_script, &QTableWidget::itemSelectionChanged, this, &WriteModeComponent::scriptTableSelected);
    connect(this->ui->tableWidget_script, &QTableWidget::itemChanged, this, &WriteModeComponent::scriptTableItemChanged);
    connect(this->ui->writeButton, &QPushButton::clicked, this, &WriteModeComponent::exportTranslateFiles);

    connect(this->ui->scriptFilterButton, &QToolButton::clicked, this->ui->scriptFilterButton, &QToolButton::showMenu);

    //全ての内容を表示
    auto showAllContents = this->ui->scriptFilterButton->addAction(tr("Show All Contents"));
    showAllContents->setCheckable(true);
    showAllContents->setChecked(true);
    //無視する内容を非表示
    auto hideIgnoreContents = this->ui->scriptFilterButton->addAction(tr("Hide Ignore Contents"));
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
        invoker invoke(this);
        if(invoke.analyze() == false){ return; }
        this->clear();
        this->setup();
        this->dispatch(StatusMessage, {baseMessage+tr(" Complete."), 3000});
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

}

WriteModeComponent::~WriteModeComponent(){
}

void WriteModeComponent::show()
{
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
}

void WriteModeComponent::treeItemSelected()
{
    auto selectedItems = this->ui->treeWidget->selectedItems();
    if(selectedItems.empty()){ return; }
    auto item = selectedItems[0];
    auto topLevelItem = getTopLevelItem(item);
    if(topLevelItem == nullptr){ return; }

    const auto treeType = topLevelItem->data(0, Qt::UserRole);
    if(treeType == TreeItemType::Main){
        auto fileName = item->data(0, Qt::UserRole).toString();
        this->ui->tabWidget->setCurrentIndex(1);
        showNormalCsvText(fileName);
    }
    else if(treeType == TreeItemType::Script)
    {
        auto changedItemScriptName = this->getScriptName(item->data(1, Qt::UserRole).toString());
        this->ui->tabWidget->setCurrentIndex(2);

        auto scriptExt = ::GetScriptExtention(this->setting->projectType);
        auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + changedItemScriptName + scriptExt;
        this->ui->scriptViewer->showFile(scriptFilePath);

        auto targetTable = this->ui->tableWidget_script;
        auto rows = targetTable->rowCount();
        if(rows <= 0){ return; }
        for(int i=0; i<rows; ++i){
            if(auto scriptNameItem = this->scriptTableItem(i, ScriptTableCol::ScriptName)){
                auto scriptName = scriptNameItem->text();
                if(scriptName.contains(changedItemScriptName)){
                    targetTable->scrollToItem(scriptNameItem, QAbstractItemView::ScrollHint::PositionAtCenter);
                    targetTable->selectRow(i);
                    break;
                }
            }
        }
    }
    else if(treeType == TreeItemType::Pictures)
    {
        auto path = item->data(1, Qt::UserRole).toString();
        if(path.isEmpty()){ return; }
        scene->clear();
        this->ui->graphicsView->verticalScrollBar()->setValue(0);
        this->ui->graphicsView->horizontalScrollBar()->setValue(0);
        QImage image(path);
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

    auto selectedItems = this->ui->treeWidget->selectedItems();
    if(selectedItems.contains(_item) == false){
        selectedItems.clear();
        selectedItems.emplace_back(_item);
    }

    auto check = _item->checkState(0);

    auto fileName = _item->data(1, Qt::UserRole).toString();
    const auto& scriptList = this->setting->fetchScriptInfo(fileName);
    auto treeCheckState = Qt::Checked;
    if(scriptList.ignore){
        treeCheckState = Qt::Unchecked;
    }
    else if(scriptList.ignorePoint.empty() == false){

    }
    if(check == Qt::Checked){
        //チェックを付ける場合は、テーブルの選択状態によってツリーのチェック状態を変更。
        auto scriptName = getScriptName(fileName);
        check = getTreeCheckStateBasedOnTable(scriptName);
    }
    std::vector<QUndoCommand *> result;
    for(auto item : selectedItems){
        if(check == treeCheckState){ continue; }
        result.emplace_back(new TreeUndo(this, item, check, treeCheckState));
    }

    if(result.empty()){ return; }
    if(_suspendHistory){
        this->discardCommand(std::move(result));
        return;
    }
    if(result.size() == 1){
        this->history->push(result[0]);
    }
    else
    {
        this->history->beginMacro(tr("Change Enable State"));
        for(auto* command : result){
            this->history->push(command);
        }
        this->history->endMacro();
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
    auto fileName = scriptNameItem->data(Qt::UserRole).toString();

    auto [textRow,textCol] = ::parseScriptWithRowCol(textPointItem->text());
    auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + fileName + GetScriptExtention(this->setting->projectType);
    this->ui->scriptViewer->showFile(scriptFilePath);

    this->ui->scriptFileName->setText(fileName);

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

    WriteDialog dialog(this->setting, this);
    if(dialog.exec() == QDialog::Rejected){ return; }

    this->setting->writeObj.exportDirectory = dialog.outputPath();
    this->setting->writeObj.exportByLanguage = dialog.writeByLanguage();

    this->ui->tabWidget->setCurrentWidget(this->ui->tab_4);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));
    invoker invoker(this);

    connect(&invoker, &invoker::getStdOut, this, [this](QString text){
        this->ui->logText->insertPlainText(text+"\n");
        this->update();
    });

    invoker.write();
    this->ui->logText->insertPlainText(tr("Done."));
}

void WriteModeComponent::setup()
{
    this->ui->tabWidget->setCurrentIndex(0);
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->treeWidget->blockSignals(true);

    //プロジェクトパスの表示
    this->ui->lineEdit->setText(this->setting->gameProjectPath);

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

    this->ui->tableWidget_script->blockSignals(false);
    this->ui->treeWidget->blockSignals(false);
}

void WriteModeComponent::setupTree()
{
    const auto translateFolder = this->setting->tempFileDirectoryPath();
    //Main
    {
        QDir sourceDir(this->setting->tempFileDirectoryPath());
        auto files = sourceDir.entryInfoList();

        auto mainItem = new QTreeWidgetItem();
        mainItem->setText(0, "Main");
        mainItem->setData(0, Qt::UserRole, TreeItemType::Main);
        this->ui->treeWidget->addTopLevelItem(mainItem);
        for(const auto& file : files)
        {
            if(file.suffix() != "json"){ continue; }

            auto fileName = file.completeBaseName();
            if(fileName == "config"){ continue; }
            else if(fileName == "Scripts"){ continue; }
            else if(fileName == "Graphics"){ continue; }

            if(QFile::exists(translateFolder + "/" + fileName + ".csv") == false){
                continue;
            }
            auto child = new QTreeWidgetItem();
            child->setText(0, fileName);
            child->setData(0, Qt::UserRole, fileName);
            mainItem->addChild(child);
        }
    }

    //Script
    {
        scriptFileNameMap.clear();
        auto scriptExt = ::GetScriptExtention(this->setting->projectType);
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

            if(writedScripts.contains(scriptFileName) == false){ continue; }
            auto child = new QTreeWidgetItem();
            child->setData(0, Qt::CheckStateRole, true);
            //チェックを外すとこのスクリプトを翻訳から除外します。
            child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
            child->setText(1, scriptName);
            child->setData(1, Qt::UserRole, file.fileName());
            child->setCheckState(0, Qt::Checked);

            const auto& scriptList = this->setting->writeObj.scriptInfo;
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&scriptFileName](const auto& script){
                return script.name.contains(scriptFileName);
            });
            if(result != scriptList.cend())
            {
                if(result->ignore){
                    child->setCheckState(0, Qt::Unchecked);
                }
                else if(result->ignorePoint.empty() == false){
                    child->setCheckState(0, Qt::PartiallyChecked);
                }
                else{
                    child->setCheckState(0, Qt::Checked);
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
        this->ui->treeWidget->addTopLevelItem(graphicsRootItem);

        QDir graphicsFolder(this->setting->tempGraphicsFileDirectoryPath());
        QFileInfoList graphFolders = graphicsFolder.entryInfoList(QDir::AllDirs|QDir::NoDotAndDotDot);

        for(const auto& graphFolder : graphFolders)
        {
            auto folderRoot = new QTreeWidgetItem();
            folderRoot->setText(0, graphFolder.baseName());
            graphicsRootItem->addChild(folderRoot);
            QFileInfoList files = QDir(graphFolder.absoluteFilePath()).entryInfoList(QStringList() << "*.*", QDir::Files);

            for(const auto& pict : files)
            {
                if(pict.size() == 0){ continue; }

                auto pictureFileName = pict.completeBaseName();
                auto child = new QTreeWidgetItem();
                child->setData(0, Qt::CheckStateRole, true);
                //チェックを外すとこのスクリプトを翻訳から除外します。
                child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
                child->setText(1, pictureFileName);
                child->setData(1, Qt::UserRole, pict.absoluteFilePath());
                child->setCheckState(0, Qt::Checked);

                auto& pixtureList = this->setting->writeObj.ignorePicturePath;
                auto result = std::find_if(pixtureList.begin(), pixtureList.end(), [&pictureFileName](const auto& pic){
                    return pic == pictureFileName;
                });
                if(result != pixtureList.end()){
                    child->setCheckState(0, Qt::Unchecked);
                }
                else {
                    child->setCheckState(0, Qt::Checked);
                }

                folderRoot->addChild(child);
            }
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
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
            return x.name == fileName && std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
        });
        return result != scriptList.cend();
    };

    const auto IsIgnoreScript = [&scriptList](QString fileName){
        fileName = QFileInfo(fileName).completeBaseName();
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
            return x.name == fileName && x.ignore;
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

    QTableWidget *targetTable = this->ui->tableWidget_script;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->setRowCount(csv.size());
    targetTable->setColumnCount(ScriptTableCol::NumCols);

    targetTable->setHorizontalHeaderLabels(QStringList() << "Include" << "ScriptName" << "TextPoint" << "Original Text");

    currentScriptWordCount = 0;
    for(int r=0; r<csv.size()-1; ++r)
    {
        const auto& line = csv[r+1];
        const auto& scriptLineInfo = line[0];
        const auto& originalText = line.size()<=1 ? "" : line[1];
        auto lineParsedResult = parseScriptNameWithRowCol(scriptLineInfo);

        const auto textColor = TextColor(lineParsedResult);

        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        targetTable->setItem(r, ScriptTableCol::EnableCheck, checkItem);

        //名前
        {
            auto* item = new QTableWidgetItem(originalText);
            item->setForeground(textColor);
            targetTable->setItem(r, ScriptTableCol::Original, item);

            checkItem->setData(Qt::CheckStateRole, IsIgnoreText(lineParsedResult) ? Qt::Unchecked : Qt::Checked);
        }

        //チェックセル以外を初期化
        auto [fileName, row, col] = lineParsedResult;
        fileName.chop(GetScriptExtention(this->setting->projectType).length());
        currentScriptWordCount += wordCountUTF8(originalText);

        {
            auto scriptName = getScriptName(fileName);
            auto* item = new QTableWidgetItem(scriptName);
            item->setForeground(textColor);
            item->setData(Qt::UserRole, fileName);
            targetTable->setItem(r, ScriptTableCol::ScriptName, item);
        }
        //テキストの位置
        {
            auto* item = new QTableWidgetItem(QString("%1:%2").arg(row).arg(col));
            item->setForeground(textColor);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);
        }
    }

    targetTable->resizeColumnsToContents();
    targetTable->update();

    this->ui->tableWidget_script->blockSignals(false);

    this->ui->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));
}


void WriteModeComponent::showNormalCsvText(QString fileName)
{
    const auto translateFolder = this->setting->tempFileDirectoryPath();
    auto csv = readCsv(translateFolder + "/" + fileName + ".csv");
    if(csv.empty()){ return; }

    this->ui->mainFileName->setText(fileName);

    const auto& header = csv[0];
    QTableWidget *targetTable = this->ui->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->setRowCount(csv.size());
    targetTable->setColumnCount(header.size());
    for(int c=0; c<header.size(); ++c){
        auto* item = new QTableWidgetItem(header[c]);
        targetTable->setHorizontalHeaderItem(c, item);
    }

    size_t wordCount = 0;
    for(int r=0; r<csv.size()-1; ++r)
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
    const auto treeType = topLevelItem->data(0, Qt::UserRole);
    if(treeType == TreeItemType::Main){ return; }

    item->setCheckState(0, check);
    if(treeType == TreeItemType::Pictures){
        writeToPictureListSetting(item, ignore);
        this->update();
        return;
    }

    writeToScriptListSetting(item, ignore);

    //テーブル側への反映
    this->ui->tableWidget_script->blockSignals(true);
    auto fileName = item->data(1, Qt::UserRole).toString();
    auto targetItemRow = fetchScriptTableSameFileRows(fileName);
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
    this->ui->tableWidget_script->blockSignals(true);
    const int row = item->row();
    auto baseCheckItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck);
    if(baseCheckItem)
    {
        writeToIgnoreScriptLine(row, check == Qt::Unchecked);
        baseCheckItem->setCheckState(check);
    }

    this->ui->tableWidget_script->blockSignals(false);

    auto scriptItem = this->scriptTableItem(row, ScriptTableCol::ScriptName);
    QString scriptName = scriptItem ? scriptItem->text() : "";

    //チェックしたアイテムに関連している、テーブル側のアイテムの色を変える
    auto rows     = fetchScriptTableSameFileRows(scriptName);
    auto treeItem = fetchScriptTreeSameFileItems(scriptName);
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
    this->ui->treeWidget->blockSignals(true);
    if(treeItem)
    {
        //同じスクリプトが全て無視されたかをチェック
        auto treeCheckState = getTreeCheckStateBasedOnTable(scriptName);
        //スクリプトの無視状態が最優先なので、チェック無しの場合は何もしない。
        if(treeItem->checkState(0) != Qt::Unchecked){
            treeItem->setCheckState(0, treeCheckState);
        }
        treeItem->setForeground(1, tableTextColorForState[treeCheckState]);
    }
    this->ui->treeWidget->blockSignals(false);

    this->update();
}

void WriteModeComponent::writeToScriptListSetting(QTreeWidgetItem *item, bool ignore)
{
    auto fileName = item->data(1, Qt::UserRole).toString();
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
    auto filePath = item->data(1, Qt::UserRole).toString();
    auto& picturePathList = this->setting->writeObj.ignorePicturePath;
    auto result = std::find_if(picturePathList.begin(), picturePathList.end(), [&filePath](const auto& path){
        return path == filePath;
    });
    if(result != picturePathList.end()){
        if(ignore){
            picturePathList.emplace_back(*result);
        }
        else {
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
        return scriptNameItem->data(Qt::UserRole).toString();
    }
    return "";
}

std::vector<int> WriteModeComponent::fetchScriptTableSameFileRows(QString scriptName)
{
    std::vector<int> result;
    const auto tableRows = this->ui->tableWidget_script->rowCount();
    for(int i=0; i<tableRows; ++i)
    {
        if(auto scriptNameItem = this->scriptTableItem(i, ScriptTableCol::ScriptName))
        {
            if(scriptName == scriptNameItem->text()){
                result.emplace_back(i);
            }
        }
    }

    return result;
}

QTreeWidgetItem* WriteModeComponent::fetchScriptTreeSameFileItems(QString scriptName)
{
    const auto numItems = this->ui->treeWidget->topLevelItemCount();
    for(int i=0; i<numItems; ++i)
    {
        auto parentItem = this->ui->treeWidget->topLevelItem(i);
        const auto treeType = parentItem->data(0, Qt::UserRole);
        if(treeType != TreeItemType::Script){ continue; }

        auto numChilds = parentItem->childCount();
        for(int c=0; c<numChilds; ++c)
        {
            auto childItem = parentItem->child(c);
            auto text = childItem->data(1, Qt::UserRole).toString();
            if(scriptName != text){ continue; }
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
