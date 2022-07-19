#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "MainComponent.h"
#include "LanguageSelectComponent.h"
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

#include <functional>

using namespace langscore;

enum TreeItemType{
    Main,
    Script,
    Pictures
};

enum ScriptTableCol : int {
    EnableCheck = 0,
    TextPoint,
    Original
};

WriteModeComponent::WriteModeComponent(Common::Type setting, MainComponent *parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::WriteModeComponent)
    , _parent(parent)
    , scene(new QGraphicsScene(this))
    , languageButtons()
    , showAllScriptContents(true)
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
    connect(this->ui->writeButton, &QPushButton::clicked, this, [this](){
        invoker invoke(this->common.obj);
        invoke.write();
        qDebug() << "Write Translate";
    });

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
        setScriptCsvText();
    });
    connect(hideIgnoreContents, &QAction::triggered, this, [this](){
        this->showAllScriptContents = false;
        setScriptCsvText();
    });

#ifdef QT_DEBUG
    auto* configView = new QPlainTextEdit(this);
    auto configTabIndex = this->ui->tabWidget->addTab(configView, "Config");

    connect(this->ui->tabWidget, &QTabWidget::currentChanged, this, [this, configTabIndex, configView](int index)
    {
        if(configTabIndex != index){ return; }

        QString text = this->common.obj->createJson();

        configView->clear();
        configView->appendPlainText(text);
    });
#endif

}

WriteModeComponent::~WriteModeComponent(){
}

void WriteModeComponent::show()
{
    this->ui->tabWidget->setCurrentIndex(0);
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->treeWidget->blockSignals(true);
    //Languages
    const std::vector<std::tuple<QLocale, QString>> languages = {
        {QLocale::English, "us.svg"},
        {QLocale::Spanish, "es.svg"},
        {QLocale::German, "de.svg"},
        {QLocale::French, "fr.svg"},
        {QLocale::Italian, "it.svg"},
        {QLocale::Japanese, "jp.svg"},
        {QLocale::Korean, "kr.svg"},
        {QLocale::Russian, "ru.svg"},
        {{QLocale::Chinese, QLocale::SimplifiedChineseScript}, "cn.svg"},
        {{QLocale::Chinese, QLocale::TraditionalChineseScript}, "cn.svg"}
    };

    if(this->common.obj->fontIndexList.empty())
    {
        auto fontExtensions = QStringList() << "*.ttf" << "*.otf";
        {
            QFileInfoList files = QDir(qApp->applicationDirPath()+"/resources/fonts").entryInfoList(fontExtensions, QDir::Files);
            for(auto& info : files){
                this->common.obj->fontIndexList.emplace_back(QFontDatabase::addApplicationFont(info.absoluteFilePath()));
            }
        }
        //プロジェクトに登録されたフォント
        {
            QFileInfoList files = QDir(this->common.obj->gameProjectPath+"/Fonts").entryInfoList(fontExtensions, QDir::Files);
            for(auto& info : files){
                this->common.obj->fontIndexList.emplace_back(QFontDatabase::addApplicationFont(info.absoluteFilePath()));
            }
        }
    }
    std::vector<QFont> fontList;
    QFont defaultFont;
    for(auto fontIndex : this->common.obj->fontIndexList){
        auto familyList = QFontDatabase::applicationFontFamilies(fontIndex);
        for(auto family : familyList)
        {
            auto familyName = family.toLower();
            if(this->common.obj->projectType == settings::VXAce && familyName.contains("vl gothic")){
                defaultFont = QFont(family);
            }
            else if(this->common.obj->projectType == settings::MV && this->common.obj->projectType == settings::MZ &&
                    familyName.contains("m+ 1m"))
            {
                defaultFont = QFont(family);
            }
            fontList.emplace_back(QFont(family));
        }
    }

    int count = 0;
    auto* buttonGroup = new QButtonGroup(this);
    for(const auto& info : languages)
    {
        auto [lang, flagName] = info;
        auto bcp47Name = lang.bcp47Name().toLower();
        if(bcp47Name == "zh"){ bcp47Name = "zh-cn"; }
        auto& langList = this->common.obj->languages;

        auto langName = lang.nativeLanguageName() + "(" + bcp47Name + ")";
        auto* langComp = new LanguageSelectComponent(":flags/"+flagName, langName, this);
        langComp->attachButtonGroup(buttonGroup);

        //プロジェクトの設定を反映
        if(this->common.obj->defaultLanguage == bcp47Name){
            langComp->setDefault(true);
        }
        auto langData = std::find(langList.begin(), langList.end(), bcp47Name);
        if(langData != langList.end()){
            langComp->setUseLang(true);
            QFont f(langData->font.name);
            f.setPixelSize(langData->font.size);
            langComp->setFontList(fontList, f);
        }
        else{
            defaultFont.setPixelSize(22);
            langComp->setFontList(fontList, defaultFont);
        }

        connect(langComp, &LanguageSelectComponent::changeUseLang, this, [this, langComp, shortName = bcp47Name](bool check)
        {
            if(check)
            {
                auto font = langComp->currentFont();
                auto& lang = this->common.obj->fetchLanguage(shortName);
                lang.font = settings::Font{font.family(), static_cast<std::uint32_t>(font.pixelSize())};
            }
            else if(check==false){
                this->common.obj->removeLanguage(shortName);
            }
        });
        connect(langComp, &LanguageSelectComponent::changeDefault, this, [this, shortName = bcp47Name](bool check){
            this->common.obj->defaultLanguage = shortName;
        });
        connect(langComp, &LanguageSelectComponent::changeFont, this, [this, shortName = bcp47Name](const QFont& font)
        {
            auto& lang = this->common.obj->fetchLanguage(shortName);
            lang.font = settings::Font{font.family(), static_cast<std::uint32_t>(font.pixelSize())};
        });

        this->ui->langTabGrid->addWidget(langComp, count/3, count%3);
        langComp->update();
        languageButtons.emplace_back(langComp);
        count++;
    }
    //通常データベース
    const auto translateFolder = this->common.obj->tempFileDirectoryPath();
    this->ui->lineEdit->setText(this->common.obj->gameProjectPath);
    {
        QDir sourceDir(this->common.obj->tempFileDirectoryPath());
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

            if(QFile::exists(translateFolder + fileName + ".csv") == false){
                continue;
            }
            auto child = new QTreeWidgetItem();
            child->setText(0, fileName);
            child->setData(0, Qt::UserRole, fileName);
            mainItem->addChild(child);
        }
    }
    //スクリプト
    {
        auto scriptCsv     = readCsv(translateFolder + "Scripts.csv");
        auto writedScripts = [&scriptCsv]()
        {
            QStringList result;
            if(scriptCsv.empty()){ return result; }
            scriptCsv.erase(scriptCsv.begin());
            for(auto& line : scriptCsv){
                auto& file = line[0];
                auto index = file.indexOf(".rb");
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
        auto scriptFolder  = this->common.obj->tempScriptFileDirectoryPath();
        auto scriptFiles   = readCsv(scriptFolder + "_list.csv");
        for(const auto& l : scriptFiles)
        {
            auto file = QFileInfo(scriptFolder+l[1]);
            if(file.suffix() != "rb"){ continue; }
            if(file.size() == 0){ continue; }

            auto filename = file.completeBaseName();
            if(writedScripts.contains(filename) == false){ continue; }
            auto child = new QTreeWidgetItem();
            child->setData(0, Qt::CheckStateRole, true);
            //チェックを外すとこのスクリプトを翻訳から除外します。
            child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
            child->setText(1, filename);
            child->setData(1, Qt::UserRole, filename);
            child->setCheckState(0, Qt::Checked);

            auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
            auto result = std::find_if(scriptList.begin(), scriptList.end(), [&filename](const auto& script){
                return script.name == filename;
            });
            if(result != scriptList.end())
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
    //画像
    {
        auto graphicsFolder  = this->common.obj->tempGraphicsFileDirectoryPath();
        auto scriptItem    = new QTreeWidgetItem();
        scriptItem->setText(0, "Pictures");
        scriptItem->setData(0, Qt::UserRole, TreeItemType::Pictures);
        this->ui->treeWidget->addTopLevelItem(scriptItem);

        QFileInfoList files = QDir(graphicsFolder).entryInfoList(QStringList() << "*.*", QDir::Files);

        for(const auto& pict : files)
        {
            if(pict.size() == 0){ continue; }

            auto filename = pict.completeBaseName();
            auto child = new QTreeWidgetItem();
            child->setData(0, Qt::CheckStateRole, true);
            //チェックを外すとこのスクリプトを翻訳から除外します。
            child->setToolTip(0, tr("Unchecking the box excludes this script from translation."));
            child->setText(1, filename);
            child->setData(1, Qt::UserRole, pict.absoluteFilePath());
            child->setCheckState(0, Qt::Checked);

            auto& pixtureList = this->common.obj->writeObj.ignorePicturePath;
            auto result = std::find_if(pixtureList.begin(), pixtureList.end(), [&filename](const auto& pic){
                return pic == filename;
            });
            if(result != pixtureList.end()){
                child->setCheckState(0, Qt::Unchecked);
            }
            else {
                child->setCheckState(0, Qt::Checked);
            }

            scriptItem->addChild(child);
        }
    }

    setScriptCsvText();

    this->ui->tableWidget_script->blockSignals(false);
    this->ui->treeWidget->blockSignals(false);
}

void WriteModeComponent::treeItemSelected()
{
    const auto translateFolder = this->common.obj->translateDirectoryPath();
    auto selectedItems = this->ui->treeWidget->selectedItems();
    if(selectedItems.empty()){ return; }
    auto item = selectedItems[0];
    if(item->parent() == nullptr){ return; }

    auto targetTable = this->ui->tableWidget;
    const auto treeType = item->parent()->data(0, Qt::UserRole);
    if(treeType == TreeItemType::Main){
        auto fileName = item->data(0, Qt::UserRole).toString();
        this->ui->tabWidget->setCurrentIndex(1);
        setNormalCsvText(fileName);
    }
    else if(treeType == TreeItemType::Script)
    {
        auto fileName = item->data(1, Qt::UserRole).toString();
        this->ui->tabWidget->setCurrentIndex(2);

        auto scriptExt = ".rb";
        if(this->common.obj->projectType == settings::MV || this->common.obj->projectType == settings::MZ){
            scriptExt = ".js";
        }
        auto scriptFilePath = this->common.obj->tempScriptFileDirectoryPath() + fileName + scriptExt;
        this->ui->scriptViewer->showFile(scriptFilePath);

        targetTable = this->ui->tableWidget_script;
        auto rows = targetTable->rowCount();
        if(rows <= 0){ return; }
        for(int i=0; i<rows; ++i){
            if(auto scriptNameItem = targetTable->item(i, 1)){
                auto scriptName = scriptNameItem->text();
                if(scriptName.contains(fileName)){
                    targetTable->scrollToItem(scriptNameItem, QAbstractItemView::ScrollHint::PositionAtCenter);
                    targetTable->selectRow(i);
                    break;
                }
            }

        }
    }
    else if(treeType == TreeItemType::Pictures)
    {
        scene->clear();
        auto path = item->data(1, Qt::UserRole).toString();
        QImage image(path);
        auto* imageItem = new QGraphicsPixmapItem(QPixmap::fromImage(image));
        scene->addItem(imageItem);
        this->ui->graphicsView->setScene(scene);

        this->ui->tabWidget->setCurrentIndex(3);
    }
}

void WriteModeComponent::treeItemChanged(QTreeWidgetItem *_item, int column)
{
    if(_item->parent() == nullptr){ return; }
    if(column != 0){ return; }

    const bool ignore = _item->checkState(0) == Qt::Unchecked;

    auto selectedItems = this->ui->treeWidget->selectedItems();
    if(selectedItems.empty()){ return; }
    if(selectedItems.contains(_item) == false){
        selectedItems.clear();
        selectedItems.emplace_back(_item);
    }

    for(auto* item : selectedItems)
    {
        const auto treeType = item->parent()->data(0, Qt::UserRole);
        if(treeType == TreeItemType::Main){ continue; }

        item->setCheckState(0, _item->checkState(0));

        if(treeType == TreeItemType::Script)
        {
            auto fileName = item->data(1, Qt::UserRole).toString();
            auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
            auto result = std::find_if(scriptList.begin(), scriptList.end(), [&fileName](const auto& script){
                return script.name == fileName;
            });
            if(result != scriptList.end()){
                result->ignore = ignore;
            }
            else{
                scriptList.emplace_back(
                    settings::WriteProps::ScriptInfo{fileName, {}, 0, ignore}
                );
            }

            //テーブル側への反映
            this->ui->tableWidget_script->blockSignals(true);
            int row = this->ui->tableWidget_script->rowCount();
            std::vector<int> targetItemRow;
            for(int i=0; i<row; ++i){
                auto tableItem = this->scriptTableItem(i, ScriptTableCol::TextPoint);
                auto text = tableItem ? tableItem->text() : "";
                if(text.indexOf(fileName) == 0){
                    targetItemRow.emplace_back(i);
                }
            }
            const auto tableCol = this->ui->tableWidget_script->columnCount();
            const auto setItemStatus = [&](QBrush textColor)
            {
                for(auto r : targetItemRow){
                    for(int c=0; c<tableCol; ++c){
                        if(auto i = this->scriptTableItem(r, c)){
                            i->setForeground(textColor);
                        }
                    }
                }
            };
            if(ignore){
                setItemStatus(Qt::gray);
            }
            else{
                setItemStatus(this->ui->tableWidget_script->palette().text());
            }
            this->ui->tableWidget_script->blockSignals(false);
        }
        else if(treeType == TreeItemType::Pictures)
        {
            auto filePath = item->data(1, Qt::UserRole).toString();
            auto& picturePathList = this->common.obj->writeObj.ignorePicturePath;
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
    }
}

void WriteModeComponent::scriptTableSelected()
{
    auto selectItems = this->ui->tableWidget_script->selectedItems();
    if(selectItems.isEmpty()){ return; }
    auto item = selectItems[0];
    int row = item->row();

    auto _item = this->scriptTableItem(row, ScriptTableCol::TextPoint);
    if(_item == nullptr){ return; }
    auto [fileName,textRow,textCol] = ::parseScriptNameWithRowCol(_item->text());
    auto scriptFilePath = this->common.obj->tempScriptFileDirectoryPath() + fileName;
    this->ui->scriptViewer->showFile(scriptFilePath);

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

    //無視リストの操作
    QStringList targetFileNames;
    const auto changeIgnoreState = [this, &targetFileNames](QTableWidgetItem *item, bool ignore)
    {
        auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
        auto filenameItem = this->scriptTableItem(item->row(), ScriptTableCol::TextPoint);
        if(filenameItem == nullptr){ return; }
        auto [filename, row, col] = parseScriptNameWithRowCol(filenameItem->text());
        auto result = std::find_if(scriptList.begin(), scriptList.end(), [&filename](const auto& script){
            return script.name == filename;
        });
        if(targetFileNames.contains(filename) == false){ targetFileNames.append(filename); }

        std::pair<size_t, size_t> rcPair = {row, col};
        //MainComponentのinvokeAnalyzeと重複
        //コンフィグの内容を変更
        if(ignore)
        {
            if(result != scriptList.end()){
                result->ignorePoint.emplace_back(rcPair);
            }
            else{
                scriptList.emplace_back(
                    settings::WriteProps::ScriptInfo{filename, {rcPair}, 0, false}
                );
            }
        }
        else
        {
            if(result != scriptList.end()){
                auto rm_result = std::remove(result->ignorePoint.begin(), result->ignorePoint.end(), rcPair);
                result->ignorePoint.erase(rm_result, result->ignorePoint.end());
            }
        }
    };

    auto selectItems = this->ui->tableWidget_script->selectedItems();
    //選択アイテムでは無いアイテムをクリックした場合、クリック側のみを処理する。
    QSet<int> rows;
    for(auto* _item : selectItems){ rows.insert(_item->row()); }
    if(rows.contains(item->row()) == false){
        rows.clear();
        rows.insert(item->row());
    }

    bool ignore = item->checkState() == Qt::Unchecked;
    this->ui->tableWidget_script->blockSignals(true);
    for(int row : rows)
    {
        if(auto _item = this->scriptTableItem(row, ScriptTableCol::EnableCheck)){
            changeIgnoreState(_item, ignore);
            _item->setCheckState(item->checkState());
        }
    }
    this->ui->tableWidget_script->blockSignals(false);

    //同じスクリプトが全て無視されたかをチェック
    auto checkState = Qt::Checked;
    int enableCheckCount = 0;
    int numFileItems = 0;
    auto tableRows = this->ui->tableWidget_script->rowCount();
    for(int i=0; i<tableRows; ++i)
    {
        auto checkItem = this->scriptTableItem(i, ScriptTableCol::EnableCheck);
        auto textPItem = this->scriptTableItem(i, ScriptTableCol::TextPoint);
        if(checkItem && textPItem)
        {
            auto [filename,r,c] = ::parseScriptNameWithRowCol(textPItem->text());
            if(targetFileNames.contains(filename) == false){ continue; }
            numFileItems++;

            if(checkItem->checkState() == Qt::Checked){
                enableCheckCount++;
            }
        }
    }
    if(numFileItems != enableCheckCount){
        checkState = (enableCheckCount == 0) ? Qt::Unchecked : Qt::PartiallyChecked;
    }

    //ツリー側のチェック状態を変更
    this->ui->treeWidget->blockSignals(true);
    auto numItems = this->ui->treeWidget->topLevelItemCount();
    for(int i=0; i<numItems; ++i){
        auto parentItem = this->ui->treeWidget->topLevelItem(i);
        const auto treeType = parentItem->data(0, Qt::UserRole);
        if(treeType != TreeItemType::Script){ continue; }

        auto numChilds = parentItem->childCount();
        for(int c=0; c<numChilds; ++c){
            auto childItem = parentItem->child(c);
            auto text = childItem->text(1);
            if(targetFileNames.filter(text).isEmpty()){
                continue;
            }
            //スクリプトの無視が最強なので、それに合わせてチェック無しだと何も変更しないようにする。
            if(childItem->checkState(0) != Qt::Unchecked){
                childItem->setCheckState(0, checkState);
            }
            childItem->setForeground(1, checkState == Qt::Unchecked ? Qt::gray : Qt::white);
            break;
        }
    }
    this->ui->treeWidget->blockSignals(false);

}

void WriteModeComponent::setNormalCsvText(QString fileName)
{
    const auto translateFolder = this->common.obj->tempFileDirectoryPath();
    auto csv = readCsv(translateFolder + fileName + ".csv");
    if(csv.empty()){ return; }

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

    for(int r=0; r<csv.size()-1; ++r)
    {
        const auto& line = csv[r+1];
        for(int c=0; c<line.size(); ++c){
            auto* item = new QTableWidgetItem(line[c]);
            targetTable->setItem(r, c, item);
        }
    }
}

void WriteModeComponent::setScriptCsvText()
{
    this->ui->tableWidget_script->blockSignals(true);
    this->ui->tableWidget_script->clear();
    const auto tempFolder = this->common.obj->tempFileDirectoryPath();
    auto csv = readCsv(tempFolder + "Scripts.csv");
    if(csv.empty()){
        this->ui->tableWidget_script->blockSignals(false);
        return;
    }

    const auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
    const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
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

    if(showAllScriptContents == false)
    {
        auto rm_result = std::remove_if(csv.begin(), csv.end(), [&IsIgnoreScript, &IsIgnoreText](const std::vector<QString>& line){
            auto [fileName, row, col] = parseScriptNameWithRowCol(line[0]);
            return IsIgnoreScript(fileName) || IsIgnoreText(fileName, row, col);
        });
        csv.erase(rm_result, csv.end());
    }

    const auto& header = csv[0];
    QTableWidget *targetTable = this->ui->tableWidget_script;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->setRowCount(csv.size());
    targetTable->setColumnCount(header.size()+1);

    targetTable->setHorizontalHeaderItem(0, new QTableWidgetItem("Include"));
    for(int c=0; c<header.size(); ++c){
        auto* item = new QTableWidgetItem(header[c]);
        targetTable->setHorizontalHeaderItem(c+1, item);
    }

    for(int r=0; r<csv.size()-1; ++r)
    {
        const auto& line = csv[r+1];
        auto [fileName, row, col] = parseScriptNameWithRowCol(line[0]);

        const auto textColor = IsIgnoreScript(fileName) ? Qt::gray : this->ui->tableWidget_script->palette().text();

        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        targetTable->setItem(r, ScriptTableCol::EnableCheck, checkItem);

        //名前
        {
            auto* item = new QTableWidgetItem(line[0]);
            item->setForeground(textColor);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);

            checkItem->setData(Qt::CheckStateRole, IsIgnoreText(fileName, row, col) ? Qt::Unchecked : Qt::Checked);
        }

        for(int c=1; c<line.size(); ++c){
            auto* item = new QTableWidgetItem(line[c]);
            item->setForeground(textColor);
            targetTable->setItem(r, c+ScriptTableCol::TextPoint, item);
        }
    }

    targetTable->resizeColumnsToContents();
    targetTable->update();

    this->ui->tableWidget_script->blockSignals(false);
}

QTableWidgetItem *WriteModeComponent::scriptTableItem(int row, int col){
    return this->ui->tableWidget_script->item(row, col);
}
