#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "LanguageSelectComponent.h"
#include "FirstWriteDialog.h"
#include "WriteDialog.h"
#include "UpdatePluginDialog.h"
#include "MessageDialog.h"
#include "ScriptViewer.h"
#include "src/invoker.h"
#include "../utility.hpp"
#include "../graphics.hpp"
#include "../csv.hpp"
#include <icu.h>

#include <QTimer>
#include <QDir>
#include <QString>
#include <QCheckBox>
#include <QLocale>
#include <QDebug>
#include <QActionGroup>
#include <QMimeData>
#include <QScrollBar>
#include <QGraphicsBlurEffect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
    Text,
    Original,
    NumCols
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

bool isJapaneseLanguage(QChar ch) {
    UErrorCode status = U_ZERO_ERROR;
    UScriptCode script = uscript_getScript(ch.unicode(), &status);

    if(U_SUCCESS(status))
    {
        switch(script)
        {
        case USCRIPT_JAPANESE:
        case USCRIPT_KATAKANA_OR_HIRAGANA:
        case USCRIPT_KATAKANA:
        case USCRIPT_HIRAGANA:
        case USCRIPT_HAN:
            return true;
            break;
        default:
            break;
        }
    }
    return false;
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
    , invokeType(InvokeType::None)
    , lastWritePath("")
    , scriptViewer(nullptr)
{
    tableTextColorForState = {
        {Qt::Checked,           QColor(0xfefefe)},
        {Qt::PartiallyChecked,  QColor(0x9a9a9a)},
        {Qt::Unchecked,         QColor(0x5a5a5a)},
    };
    this->addDispatch(this);

    ui->setupUi(this);
    {
        this->scriptViewer = new ScriptViewer(this, this);
        auto index = this->ui->splitter->indexOf(this->ui->scriptViewer);
        this->ui->splitter->replaceWidget(index, this->scriptViewer);
        delete this->ui->scriptViewer;
        this->ui->scriptViewer = this->scriptViewer;
    }

    this->ui->modeDesc->hide();
    this->ui->logText->SetSettings(this->setting);
    this->setAcceptDrops(true);

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
    connect(this->ui->writeCSVAndPluginButton, &QPushButton::clicked, this, &WriteModeComponent::firstExportTranslateFiles);
    connect(this->ui->writeButton, &QPushButton::clicked, this, &WriteModeComponent::exportTranslateFiles);
    connect(this->ui->updatePluginButton, &QPushButton::clicked, this, &WriteModeComponent::exportPlugin);

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


    connect(this->ui->autoCheckButton, &QToolButton::clicked, this->ui->autoCheckButton, &QToolButton::showMenu);
    {
        //記号のみの文のチェックを外す
        auto uncheckSignOnly = new QAction(tr("Uncheck Sign Only Text"));
        this->ui->autoCheckButton->addAction(uncheckSignOnly);

        connect(uncheckSignOnly, &QAction::triggered, this, [this](){
            auto numRow = this->ui->tableWidget_script->rowCount();
            for(int r=0; r<numRow; ++r)
            {
                auto text = this->ui->tableWidget_script->item(r, ScriptTableCol::Original)->text();
                bool is_find = false;
                for(QChar qc : text) {
                    int c = qc.toLatin1();
                    if(c == 0){
                        c = qc.unicode();
                        if(c == 0){ continue; }
                    }
                    if(c < 127) {
                       is_find |= (std::isalpha(c) != 0);
                    }
                    else {
                       is_find = true;
                    }
                }
                if(is_find == false){
                    this->ui->tableWidget_script->item(r, ScriptTableCol::EnableCheck)->setCheckState(Qt::Unchecked);
                }
            }
        });

        //プログラミングで使用される文のチェックを外す
        // auto uncheckProgrammingText = new QAction(tr("Uncheck Text used in programming"));
        // this->ui->autoCheckButton->addAction(uncheckProgrammingText);
        // connect(uncheckProgrammingText, &QAction::triggered, this, [this]()
        // {
        //     auto numRow = this->ui->tableWidget_script->rowCount();
        //     std::vector<QString> ignoreTextList = {
        //         "BigInt", "String", "Boolean", "Number", "Symbol"
        //     };
        //     for(int r=0; r<numRow; ++r)
        //     {
        //         auto text = this->ui->tableWidget_script->item(r, ScriptTableCol::Original)->text();
        //         bool is_find = false;
        //         for(QChar qc : text) {
        //             int c = qc.toLatin1();
        //             if(c == 0){
        //                 c = qc.unicode();
        //                 if(c == 0){ continue; }
        //             }
        //             if(c < 127) {
        //                 is_find |= (std::isalpha(c) != 0);
        //             }
        //             else {
        //                 is_find = true;
        //             }
        //         }
        //         if(is_find == false){
        //             this->ui->tableWidget_script->item(r, ScriptTableCol::EnableCheck)->setCheckState(Qt::Unchecked);
        //         }
        //     }
        // });

        //日本語を含まない文章のチェックを外す
        auto uncheckNotIncludeHiragana = new QAction(tr("Uncheck text that does not contain hiragana"));
        this->ui->autoCheckButton->addAction(uncheckNotIncludeHiragana);
        connect(uncheckNotIncludeHiragana, &QAction::triggered, this, [this]()
        {
            auto numRow = this->ui->tableWidget_script->rowCount();
            for(int r=0; r<numRow; ++r)
            {
                auto text = this->ui->tableWidget_script->item(r, ScriptTableCol::Original)->text();
                bool is_find = false;
                for(QChar qc : text) {
                    is_find |= ::isJapaneseLanguage(qc);
                    if(is_find){ break; }
                }
                if(is_find == false){
                    this->ui->tableWidget_script->item(r, ScriptTableCol::EnableCheck)->setCheckState(Qt::Unchecked);
                }
            }
        });
    }

    connect(this->ui->updateButton, &QPushButton::clicked, this, [this]()
    {
        MessageDialog mes(this);
        //更新する
        mes.okButtonText(tr("Update"));
        //更新すると表示されている内容が最新の内容に変更されます。
        //この処理は元に戻せません。
        //※翻訳ファイルは書き出しを行うまで変更されません。
        mes.setLabelText(tr("When updated, the displayed content will be changed to the latest content.\nThis process cannot be undone.\nNote:The translation file will not be changed until it is exported."));
        this->setGraphicsEffect(new QGraphicsBlurEffect);
        this->changeEnabledUIState(false);
        auto result = mes.exec();
        this->setGraphicsEffect(nullptr);
        if(result != QDialog::Accepted){
            this->changeEnabledUIState(true);
            return;
        }

        this->ui->tabWidget->setCurrentWidget(this->ui->tab_4);
        this->ui->logText->clear();
        this->ui->logText->insertPlainText(tr("Update Projects...\n"));

        auto baseMessage = tr("Update to the latest content...");
        this->dispatch(StatusMessage, {baseMessage});
        this->dispatch(SaveProject,{});
        this->_invoker->reanalysis();

        invokeType = InvokeType::Reanalisys;
    });

    connect(_invoker, &invoker::getStdOut, this->ui->logText, &InvokerLogViewer::writeText);
    connect(_invoker, &invoker::update, this, [this](){ this->update(); });
    connect(_invoker, &invoker::finish, this, [this](int)
    {
        this->changeEnabledUIState(true);
        this->ui->logText->insertPlainText(tr("Done."));

        if(invokeType == InvokeType::Reanalisys){
            this->clear();
            this->setup();
            this->dispatch(StatusMessage, {tr(" Complete."), 3000});
        }
        else if(invokeType == InvokeType::ExportFirstTime || invokeType == InvokeType::ExportCSV || invokeType == InvokeType::Reanalisys){
            QProcess::startDetached("explorer", {QDir::toNativeSeparators(lastWritePath)});
            this->setting->isFirstExported = true;

            this->ui->writeCSVAndPluginButton->setVisible(false);
            this->ui->updatePluginButton->setVisible(true);
            this->ui->writeButton->setVisible(true);

            this->dispatch(SaveProject,{});
        }
        invokeType = InvokeType::None;
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
    this->removeDispatch(this);
}

void WriteModeComponent::show()
{
    const auto& analyzeDirPath = this->setting->analyzeDirectoryPath();
    auto analyzeDir = QDir(analyzeDirPath);
    auto basicFiles = analyzeDir.entryInfoList(QStringList{"*.json"}, QDir::Files);
    for(auto& file : basicFiles){
        auto fileName = file.completeBaseName();
        if(fileName.isEmpty() == false){
            this->setting->fetchBasicDataInfo(fileName);
        }
    }

    //auto writedScriptList = [&analyzeDirPath]()
    //{
    //    QStringList result;
    //    auto scriptCsv     = readCsv(analyzeDirPath + "/Scripts.lsjson");
    //    if(scriptCsv.empty()){ return result; }
    //    scriptCsv.erase(scriptCsv.begin()); //Headerを削除
    //    for(auto& line : scriptCsv){
    //        result.emplace_back(line[0]);
    //    }
    //    result.sort();
    //    result.erase(std::unique(result.begin(), result.end()), result.end());

    //    return result;
    //}();

    //for(auto& script : writedScriptList)
    //{
    //    auto parseResult = parseScriptNameWithRowCol(script);
    //    QString fileName;
    //    size_t row = 0;
    //    size_t col = 0;
    //    if(std::holds_alternative<TextPosition::RowCol>(parseResult.d)){
    //        fileName = parseResult.scriptFileName;
    //        const auto& cell = std::get<TextPosition::RowCol>(parseResult.d);
    //        row = cell.row;
    //        col = cell.col;
    //    }
    //    else{
    //        continue;
    //    }

    //    auto& scriptList = this->setting->writeObj.scriptInfo;
    //    const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
    //        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
    //            return x.name == fileName &&  std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
    //        });
    //        return result != scriptList.cend();
    //    };
    //    if(IsIgnoreText(fileName, row, col) == false){ continue; }

    //    auto& info = this->setting->fetchScriptInfo(fileName);
    //    if(std::find_if(info.ignorePoint.begin(), info.ignorePoint.end(), [row, col](const auto& x){
    //        return x == std::pair<size_t,size_t>{row, col};
    //    }) == info.ignorePoint.end())
    //    {
    //        settings::TextPoint point;
    //        point.row = row;
    //        point.col = col;
    //        info.ignorePoint.emplace_back(std::move(point));
    //    }
    //}

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
    this->ui->tableWidget->setRowCount(0);
    this->ui->tableWidget_script->clear();
    this->scriptViewer->clear();
    for(auto langButton : this->languageButtons){
        this->ui->langTabGrid->removeWidget(langButton);
        delete langButton;
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
        showNormalJsonText(item->text(TreeColIndex::Name), fileName);
    }
    else if(treeType == TreeItemType::Script)
    {
        auto changedItemScriptFileName = ::getFileName(item);
        this->ui->tabWidget->setCurrentIndex(2);

        auto scriptExt = ::GetScriptExtension(this->setting->projectType);
        auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + changedItemScriptFileName + scriptExt;
        this->scriptViewer->showFile(scriptFilePath);

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
    this->scriptViewer->showFile(scriptFilePath);

    auto scriptName = ::getScriptName(scriptNameItem);
    this->ui->scriptFileName->setText(scriptName + "(" + fileName + ")");

    //スクリプトのスクロール
    int textLen = 0;
    if(auto i = this->scriptTableItem(row, ScriptTableCol::Original)){
        auto text = i->text();
        textLen = text.length();
    }
    if(textRow != std::numeric_limits<size_t>::max()){
        this->scriptViewer->scrollWithHighlight(textRow, textCol, textLen);
    }
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

void WriteModeComponent::exportPlugin()
{
    if(this->setting->writeObj.exportDirectory.isEmpty()){
        this->setting->writeObj.exportDirectory = this->setting->translateDirectoryPath();
    }
    this->setGraphicsEffect(new QGraphicsBlurEffect);
    this->changeEnabledUIState(false);

    UpdatePluginDialog dialog(this->setting, this);

    auto execCode = dialog.exec();
    this->setGraphicsEffect(nullptr);
    if(execCode == QDialog::Rejected){
        this->changeEnabledUIState(true);
        return;
    }

    QDir lsProjDir(this->setting->langscoreProjectDirectory);
    auto relativePath = lsProjDir.relativeFilePath(lastWritePath);
    this->setting->writeObj.exportDirectory     = relativePath;
    this->setting->writeObj.enableLanguagePatch = dialog.isEnableLanguagePatch();
    this->setting->setPackingDirectory(relativePath);

    this->ui->tabWidget->setCurrentWidget(this->ui->tab_4);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Update Plugin...\n"));

    this->dispatch(SaveProject,{});
    _invoker->updatePlugin();
    invokeType = InvokeType::UpdatePlugin;
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
    if(execCode == QDialog::Rejected){
        this->changeEnabledUIState(true);
        return;
    }

    QDir lsProjDir(this->setting->langscoreProjectDirectory);
    lastWritePath = dialog.outputPath();
    auto relativePath = lsProjDir.relativeFilePath(lastWritePath);
    this->setting->writeObj.exportDirectory = relativePath;
    this->setting->writeObj.enableLanguagePatch = dialog.writeByLanguagePatch();
    this->setting->writeObj.writeMode = dialog.writeMode();
    this->setting->setPackingDirectory(relativePath);

    if(dialog.backup()){ backup(); }

    this->ui->tabWidget->setCurrentWidget(this->ui->tab_4);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));

    this->dispatch(SaveProject,{});
    _invoker->exportCSV();
    invokeType = InvokeType::ExportCSV;

}

void WriteModeComponent::firstExportTranslateFiles()
{
    if(this->setting->writeObj.exportDirectory.isEmpty()){
        this->setting->writeObj.exportDirectory = this->setting->translateDirectoryPath();
    }
    this->setGraphicsEffect(new QGraphicsBlurEffect);
    this->changeEnabledUIState(false);

    FirstWriteDialog dialog(this->setting, this);

    auto execCode = dialog.exec();
    this->setGraphicsEffect(nullptr);
    if(execCode == QDialog::Rejected){
        this->changeEnabledUIState(true);
        return;
    }

    QDir lsProjDir(this->setting->langscoreProjectDirectory);
    lastWritePath = dialog.outputPath();
    auto relativePath = lsProjDir.relativeFilePath(lastWritePath);
    this->setting->writeObj.exportDirectory = relativePath;
    this->setting->writeObj.enableLanguagePatch = dialog.writeByLanguagePatch();
    this->setting->writeObj.writeMode = dialog.writeMode();
    this->setting->setPackingDirectory(relativePath);

    if(dialog.backup()){ backup(); }

    this->ui->tabWidget->setCurrentWidget(this->ui->tab_4);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files(First)...\n"));

    this->dispatch(SaveProject,{});
    _invoker->exportFirstTime();
    invokeType = InvokeType::ExportFirstTime;
}

void WriteModeComponent::setup()
{
    this->ui->tabWidget->setCurrentIndex(0);
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->treeWidget->blockSignals(true);

    //プロジェクトパスの表示
    this->ui->projectPath->setText(this->setting->gameProjectPath);
    this->ui->lsProjPath->setText(this->setting->langscoreProjectDirectory);

    this->setting->setupFontList();

    //Languages
    const std::vector<QLocale> languages = {
        {QLocale::Japanese},
        {QLocale::English},
        {QLocale::Chinese, QLocale::SimplifiedChineseScript},
        {QLocale::Chinese, QLocale::TraditionalChineseScript},
        {QLocale::Korean},
        {QLocale::Spanish},
        {QLocale::German},
        {QLocale::French},
        {QLocale::Italian},
        {QLocale::Russian},
    };
    this->setting->setupLanguages(languages);

    int count = 0;
    auto* buttonGroup = new QButtonGroup(this);
    for(const auto& lang : languages)
    {
        auto langComp = new LanguageSelectComponent(lang, this, this);
        langComp->attachButtonGroup(buttonGroup);
        this->ui->langTabGrid->addWidget(langComp, count/3, count%3);
        langComp->update();
        connect(langComp, &LanguageSelectComponent::ChangeUseLanguageState, this, [this](){
            auto isDisabledALl = std::all_of(this->setting->languages.begin(), this->setting->languages.end(), [](const settings::Language& lang){
                return lang.enable == false;
            });
            if(isDisabledALl){
                this->ui->writeButton->setEnabled(false);
                //CSVを書き出すには言語を選択して下さい
                this->ui->writeButton->setText(tr("To export CSV, please select a language"));
            }
            else{
                this->ui->writeButton->setEnabled(true);
                this->ui->writeButton->setText(QCoreApplication::translate("WriteModeComponent", "Write Translate CSV", nullptr));
            }
        });
        languageButtons.emplace_back(langComp);
        count++;
    }

    setupTree();
    setupScriptCsvText();

    this->changeUIColor();

    this->setting->saveForProject();

    this->ui->tableWidget_script->blockSignals(false);
    this->ui->treeWidget->blockSignals(false);

    if(setting->isFirstExported == false){
        this->ui->writeCSVAndPluginButton->setVisible(true);
        this->ui->updatePluginButton->setVisible(false);
        this->ui->writeButton->setVisible(false);
    }
    else{
        this->ui->writeCSVAndPluginButton->setVisible(false);
        this->ui->updatePluginButton->setVisible(true);
        this->ui->writeButton->setVisible(true);
    }

    this->update();
}

void WriteModeComponent::setupTree()
{
    const auto analyzeFolder = this->setting->analyzeDirectoryPath();
    //Main
    {
        auto mapInfosCsv = readCsv(analyzeFolder + "/MapInfos.csv");
        QDir sourceDir(this->setting->analyzeDirectoryPath());
        auto files = sourceDir.entryInfoList();

        auto mainItem = new QTreeWidgetItem();
        mainItem->setText(0, "Main");
        mainItem->setData(0, Qt::UserRole, TreeItemType::Main);
        this->ui->treeWidget->addTopLevelItem(mainItem);
        for(const auto& file : files)
        {
            if(file.suffix() != "json"){ continue; }

            auto fileViewName = file.completeBaseName();
            if(fileViewName == "config"){ continue; }
            else if(fileViewName == "Scripts"){ continue; }
            else if(fileViewName == "Graphics"){ continue; }

            if(QFile::exists(analyzeFolder + "/" + fileViewName + ".lsjson") == false){
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
        QFile jsonFile(analyzeFolder + "/Scripts.lsjson");

        if(false == jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            qWarning() << "Failed to open JSON file:" << jsonFile.fileName();
            return;
        }

        QByteArray jsonData = jsonFile.readAll();
        jsonFile.close();

        QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
        if(jsonDoc.isNull() || !jsonDoc.isArray()) {
            qWarning() << "Invalid JSON format in file:" << jsonFile.fileName();
            return;
        }

        QJsonArray jsonArray = jsonDoc.array();
        auto writedScripts = [&jsonArray, &scriptExt]() {
            std::vector<QString> result;
            for(const QJsonValue& value : jsonArray) {
                QJsonObject obj = value.toObject();
                QString file = obj["file"].toString();
                if(file.isEmpty() == false) {
                    result.emplace_back(std::move(file));
                }
            }
            std::ranges::sort(result);
            result.erase(std::unique(result.begin(), result.end()), result.end());
            return result;
        }();

        auto scriptItem    = new QTreeWidgetItem();
        scriptItem->setText(0, "Script");
        scriptItem->setData(0, Qt::UserRole, TreeItemType::Script);
        this->ui->treeWidget->addTopLevelItem(scriptItem);
        auto scriptFolder  = this->setting->tempScriptFileDirectoryPath();
        std::vector<std::vector<QString>> scriptFiles;
        if(setting->projectType == settings::VXAce){
            scriptFiles  = readCsv(scriptFolder + "/_list.csv");
        }
        else if(setting->projectType == settings::MV || setting->projectType == settings::MZ){

            QDir folder(this->setting->tempScriptFileDirectoryPath());
            QFileInfoList files = folder.entryInfoList({"*.js"});
            for(const auto &file : files)
            {
                auto filename = file.completeBaseName();
                scriptFiles.emplace_back(std::vector<QString>{filename, filename});
            }

        }
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
        this->ui->treeWidget->addTopLevelItem(graphicsRootItem);

        QDir graphicsFolder(this->setting->tempGraphicsFileDirectoryPath());
        QFileInfoList graphFolders = graphicsFolder.entryInfoList(QDir::AllDirs|QDir::NoDotAndDotDot);

        for(const auto& graphFolder : graphFolders)
        {
            auto folderRoot = new QTreeWidgetItem();
            folderRoot->setText(TreeColIndex::Name, graphFolder.baseName());
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

void WriteModeComponent::changeUIColor()
{
    QColor topLevelItemBGColor;
    QColor graphicFolderBGColor;
    QColor itemTextColor;

    auto theme = this->getColorTheme().getCurrentTheme();
    if(theme == ColorTheme::Dark)
    {
        topLevelItemBGColor = QColor(128, 128, 128, 90);
        graphicFolderBGColor = QColor(128, 128, 128, 90);
        itemTextColor = Qt::white;

        tableTextColorForState = {
            {Qt::Checked,           QColor(0xfefefe)},
            {Qt::PartiallyChecked,  QColor(0x9a9a9a)},
            {Qt::Unchecked,         QColor(0x5a5a5a)},
        };
    }
    else{
        topLevelItemBGColor = QColor(255, 255, 255, 128);
        graphicFolderBGColor = QColor(255, 255, 255, 128);
        itemTextColor = Qt::black;

        tableTextColorForState = {
            {Qt::Checked,           QColor(0x161616)},
            {Qt::PartiallyChecked,  QColor(0x646464)},
            {Qt::Unchecked,         QColor(0x9a9a9a)},
        };
    }

    this->ui->treeWidget->blockSignals(true);
    auto numToplevelItem = this->ui->treeWidget->topLevelItemCount();
    for(int i=0; i<numToplevelItem; ++i)
    {
        auto item = this->ui->treeWidget->topLevelItem(i);
        item->setBackground(0, topLevelItemBGColor);
        item->setBackground(1, topLevelItemBGColor);
        item->setForeground(0, itemTextColor);
    }
    this->ui->treeWidget->blockSignals(false);
    this->ui->treeWidget->update();


    this->ui->tableWidget_script->blockSignals(true);
    auto rows = this->ui->tableWidget_script->rowCount();
    for(int r=0; r<rows; ++r)
    {
        if(auto scriptNameItem = this->ui->tableWidget_script->item(r, ScriptTableCol::ScriptName)) {
            auto scriptName = scriptNameItem->text();
            auto treeCheckState = getTreeCheckStateBasedOnTable(scriptName);
            setTableItemTextColor(r, tableTextColorForState[treeCheckState]);
        }
    }
    this->ui->tableWidget_script->blockSignals(false);
    this->ui->tableWidget_script->update();


    {
        auto icon = this->ui->scriptFilterButton->icon();
        auto image = icon.pixmap(this->ui->scriptFilterButton->size()).toImage();
        if(this->getColorTheme().getCurrentTheme() == ColorTheme::Dark){
            graphics::ReverceHSVValue(image);
        }
        this->ui->scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));
        this->ui->scriptFilterButton->update();
    }

}

void WriteModeComponent::setupScriptCsvText()
{
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->tableWidget_script->clear();
    const auto tempFolder = this->setting->analyzeDirectoryPath();

    QFile jsonFile(tempFolder + "/Scripts.lsjson");

    if(false == jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open JSON file:" << jsonFile.fileName();
        return;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << jsonFile.fileName();
        return;
    }

    struct ScriptTextData {
        QString text;
        TextPosition pos;
    };

    QJsonArray jsonArray = jsonDoc.array();
    auto writedScripts = [&jsonArray]() 
    {
        std::vector<ScriptTextData> result;
        for(const QJsonValue& value : jsonArray) 
        {
            TextPosition pos;
            QJsonObject obj = value.toObject();
            pos.scriptFileName = obj["file"].toString();
            if(obj.contains("parameterName")) 
            {
                pos.type = TextPosition::Type::Argument;
                if(obj.contains("value")) {
                    pos.value = obj["value"].toString();
                }
                TextPosition::ScriptArg rowCol;
                rowCol.valueName = obj["parameterName"].toString();
                pos.d = rowCol;

                ScriptTextData data = {
                    obj["original"].toString(),
                    pos
                };

                result.emplace_back(std::move(data));
            }
            else {
                pos.type = TextPosition::Type::RowCol;
                if(obj.contains("value")) {
                    pos.value = obj["value"].toString();
                }
                TextPosition::RowCol rowCol;
                rowCol.row = obj["row"].toInteger();
                rowCol.col = obj["col"].toInteger();
                pos.d = rowCol;

                ScriptTextData data = {
                    obj["original"].toString(),
                    pos
                };

                result.emplace_back(std::move(data));
            }
        }
        return result;
    }();

    if(writedScripts.empty()){
        this->ui->tableWidget_script->blockSignals(false);
        return;
    }

    using ScriptLineInfo = std::tuple<QString, size_t, size_t>;

    const auto& scriptList = this->setting->writeObj.scriptInfo;
    const auto IsIgnoreText = [&scriptList](const TextPosition& info)
    {
        auto fileName = info.scriptFileName;
        fileName = withoutExtension(std::move(fileName));
        if(std::holds_alternative<TextPosition::RowCol>(info.d))
        {
            auto& cell = std::get<TextPosition::RowCol>(info.d);
            auto cellPos = std::pair<size_t, size_t>{cell.row, cell.col};
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
                return withoutExtension(x.name) == fileName && std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), cellPos) != x.ignorePoint.cend();
            });
            return result != scriptList.cend();
        }
        else
        {
            auto& cell = std::get<TextPosition::ScriptArg>(info.d);
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
                return withoutExtension(x.name) == fileName && std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), cell.valueName) != x.ignorePoint.cend();
            });
            return result != scriptList.cend();
        }
    };

    const auto IsIgnoreScript = [&scriptList](QString fileName){
        fileName = withoutExtension(std::move(fileName));
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
            return withoutExtension(x.name) == fileName && x.ignore;
        });
        return result != scriptList.cend();
    };

    const auto TextColor = [&](const TextPosition& lineInfo){
        if(IsIgnoreScript(lineInfo.scriptFileName)){ return tableTextColorForState[Qt::Unchecked]; }
        if(IsIgnoreText(lineInfo)){ return tableTextColorForState[Qt::PartiallyChecked]; }
        return tableTextColorForState[Qt::Checked];
    };

    if(showAllScriptContents == false)
    {
        auto rm_result = std::remove_if(writedScripts.begin()+1, writedScripts.end(), [&IsIgnoreScript, &IsIgnoreText](const ScriptTextData& data){
            if(data.pos.scriptFileName.isEmpty()) { return false; }
            return IsIgnoreScript(data.pos.scriptFileName) || IsIgnoreText(data.pos);
        });
        writedScripts.erase(rm_result, writedScripts.end());
    }

    // 常に非表示にするスクリプト名のフィルタリング（最適化版）
    {
        std::vector<QString> hideScriptNameList = {"langscore_custom", "langscore"};

        // スクリプト名とその行位置を一時的にマッピング
        std::unordered_map<QString, std::vector<size_t>> scriptNameToRowMap;

        // すべての行を走査して一度だけマッピングを作成
        for(size_t i = 1; i < writedScripts.size(); ++i)
        {

            auto scriptName = getScriptName(writedScripts[i].pos.scriptFileName);

            if(scriptNameToRowMap.find(scriptName) == scriptNameToRowMap.end()) {
                scriptNameToRowMap[scriptName] = std::vector<size_t>{i};
            }
            else {
                scriptNameToRowMap[scriptName].push_back(i);
            }
        }

        // 削除対象となる行番号を収集
        std::vector<size_t> rowsToRemove;
        for(const auto& hideName : hideScriptNameList) {
            auto it = scriptNameToRowMap.find(hideName);
            if(it != scriptNameToRowMap.end()) {
                // 該当スクリプト名のすべての行を削除リストに追加
                for(auto rowIndex : it->second) {
                    rowsToRemove.push_back(rowIndex);
                }
            }
        }

        // 行番号を降順にソートして削除（後ろから削除していくことでインデックスのずれを防ぐ）
        if(rowsToRemove.empty() == false) {
            std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<size_t>());

            for(auto rowIndex : rowsToRemove) {
                writedScripts.erase(writedScripts.begin() + rowIndex);
            }
        }
    }

    //ヘッダーを除外するので-1
    const auto numTableItems = writedScripts.size()-1;
    QTableWidget *targetTable = this->ui->tableWidget_script;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // デフォルトの最小高さを設定して、内容が少ない場合でも最低限の高さを確保
    targetTable->verticalHeader()->setMinimumSectionSize(28);
    targetTable->setRowCount(numTableItems);
    targetTable->setColumnCount(ScriptTableCol::NumCols);

    targetTable->setHorizontalHeaderLabels(QStringList() << "Include" << "ScriptName" << "TextPoint" << "Text" << "Original Text");

    currentScriptWordCount = 0;
    const auto& scriptExt = GetScriptExtension(this->setting->projectType);
    for(int r=0; r<numTableItems; ++r)
    {
        auto& scriptData = writedScripts[r+1];
        const auto& originalText   = scriptData.text;
        auto& lineParsedResult = scriptData.pos;

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
        if(lineParsedResult.scriptFileName.contains(scriptExt)){
            lineParsedResult.scriptFileName.chop(scriptExt.length());
        }
        currentScriptWordCount += wordCountUTF8(originalText);

        {
            auto scriptName = getScriptName(lineParsedResult.scriptFileName);
            auto* item = new QTableWidgetItem(scriptName);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            item->setData(Qt::UserRole, withoutExtension(lineParsedResult.scriptFileName));
            targetTable->setItem(r, ScriptTableCol::ScriptName, item);
            if(scriptNameToTableIndexMap.find(scriptName) == scriptNameToTableIndexMap.end()) {
                scriptNameToTableIndexMap.emplace(scriptName, std::vector<int>{r});
            }
            else {
                scriptNameToTableIndexMap[scriptName].emplace_back(r);
            }
        }
        //テキストの位置
        if(std::holds_alternative<TextPosition::RowCol>(lineParsedResult.d))
        {
            auto& cell = std::get<TextPosition::RowCol>(lineParsedResult.d);
            auto* item = new QTableWidgetItem(QString("%1:%2").arg(cell.row).arg(cell.col));
            if(cell.row == 0 && cell.col == 0){
                item->setText("Parameter");
            }
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);
        }
        else{
            auto& cell = std::get<TextPosition::ScriptArg>(lineParsedResult.d);
            auto* item = new QTableWidgetItem(cell.valueName);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);
        }

        {

            auto* item = new QTableWidgetItem(lineParsedResult.value);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::Text, item);
        }
    }

    targetTable->resizeColumnsToContents();
    targetTable->update();

    this->ui->tableWidget_script->blockSignals(false);

    this->ui->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));
}

void WriteModeComponent::showNormalJsonText(QString treeItemName, QString fileName)
{
    const auto translateFolder = this->setting->analyzeDirectoryPath();
    QFile file(translateFolder + "/" + withoutExtension(fileName) + ".lsjson");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << file.fileName();
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << file.fileName();
        return;
    }

    QJsonArray jsonArray = jsonDoc.array();
    if(jsonArray.isEmpty()) {
        qWarning() << "JSON array is empty in file:" << file.fileName();
        return;
    }

    this->ui->mainFileName->setText(treeItemName);

    // テーブルの準備
    QTableWidget* targetTable = this->ui->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->setRowCount(jsonArray.size());
    targetTable->setColumnCount(2); // "original" と "type" の2列


    // ヘッダー設定
    targetTable->setHorizontalHeaderLabels(QStringList() << tr("Original") << tr("Type"));

    size_t wordCount = 0;
    for(int r = 0; r < jsonArray.size(); ++r) {
        QJsonObject item = jsonArray.at(r).toObject();

        // Original テキスト
        QString originalText = item["original"].toString();
        wordCount += wordCountUTF8(originalText);
        auto* originalItem = new QTableWidgetItem(originalText);
        targetTable->setItem(r, 0, originalItem);

        // Type (配列の場合があるので結合して表示)
        QJsonArray typeArray = item["type"].toArray();
        QStringList typeList;
        for(const QJsonValue& typeValue : typeArray) {
            typeList.append(typeValue.toString());
        }
        QString typeText = typeList.join(", ");
        auto* typeItem = new QTableWidgetItem(typeText);
        targetTable->setItem(r, 1, typeItem);
    }

    this->ui->mainFileWordCount->setText(tr("Word Count : %1").arg(wordCount));

    // 強制的にテーブルを更新して行の高さを調整
    targetTable->resizeRowsToContents();

    // 遅延処理でサイズの再調整を行う
    QTimer::singleShot(0, this, [targetTable]() {
        targetTable->setUpdatesEnabled(false);
        targetTable->resizeRowsToContents();
        targetTable->setUpdatesEnabled(true);
        targetTable->update();
    });
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
    std::vector<settings::Font> fonts;
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
            this->setting->fontIndexList.emplace_back(settings::Local, fontIndex, appFont, destPath);
            fonts.emplace_back(settings::Font{appFont, destPath, QFont(appFont)});
        }
    }
    for(auto* langButton : languageButtons){
        langButton->setSelectableFontList(fonts, "");
    }

}

void WriteModeComponent::changeEnabledUIState(bool enable)
{
    this->ui->treeWidget->setEnabled(enable);
    this->ui->tableWidget_script->setEnabled(enable);
    this->ui->writeButton->setEnabled(enable);
    this->ui->updateButton->setEnabled(enable);
}

void WriteModeComponent::receive(DispatchType type, const QVariantList &args)
{
    if(type == ComponentBase::ChangeColor){
        if(args.empty()){ return; }

        bool isOk = false;
        args[0].toInt(&isOk);
        if(isOk == false){ return; }

        this->changeUIColor();
    }
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
    this->setFontList(std::move(fontPathList));
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
    if(scriptNameToTableIndexMap.find(scriptName) == scriptNameToTableIndexMap.end()) {
        result = scriptNameToTableIndexMap[scriptName];
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
    else if(this->setting->projectType == settings::MZ || this->setting->projectType == settings::MV){
        backupFiles = {
            "/data/translate",
            "/js/plugins.js"
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
