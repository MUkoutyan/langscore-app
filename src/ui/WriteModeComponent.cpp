#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "MainComponent.h"
#include "../csv.hpp"

#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QCheckBox>
#include <QLocale>

using namespace langscore;

enum TreeItemType{
    Main,
    Script,
    Pictures
};

WriteModeComponent::WriteModeComponent(Common::Type setting, MainComponent *parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::WriteModeComponent)
    , _parent(parent)
    , scene(new QGraphicsScene(this))
{
    ui->setupUi(this);

    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, [this]()
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
            targetTable = this->ui->tableWidget_script;
            this->ui->tabWidget->setCurrentIndex(2);

            auto scriptFilePath = this->common.obj->tempScriptFileDirectoryPath() + fileName + ".rb";
            this->ui->scriptViewer->showFile(scriptFilePath);
            if(targetTable->rowCount() != 0){ return; }
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
    });

    connect(this->ui->treeWidget, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *item, int column)
    {
        if(item->parent() == nullptr){ return; }
        if(column != 0){ return; }

        const auto treeType = item->parent()->data(0, Qt::UserRole);
        if(treeType == TreeItemType::Main){
            auto fileName = item->data(0, Qt::UserRole).toString();
        }
        else if(treeType == TreeItemType::Script)
        {
            const bool ignore = item->checkState(0) == Qt::Unchecked;
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
                auto tableItem = this->ui->tableWidget_script->item(i, 1);
                auto text = tableItem ? tableItem->text() : "";
                if(text.indexOf(fileName) == 0){
                    targetItemRow.emplace_back(i);
                }
            }
            const auto tableCol = this->ui->tableWidget_script->columnCount();
            if(ignore)
            {
                for(auto r : targetItemRow){
                    for(int c=0; c<tableCol; ++c){
                        this->ui->tableWidget_script->item(r, c)->setFlags(item->flags()&~Qt::ItemIsEnabled);
                    }
                }
            }
            else{
                for(auto r : targetItemRow){
                    for(int c=0; c<tableCol; ++c){
                        this->ui->tableWidget_script->item(r, c)->setFlags(item->flags()|Qt::ItemIsEnabled);
                    }
                }
            }
            this->ui->tableWidget_script->blockSignals(false);
        }
        else if(treeType == TreeItemType::Script)
        {
            const bool ignore = item->checkState(0) == Qt::Unchecked;
            auto filePath = item->data(1, Qt::UserRole).toString();
            auto& picturePathList = this->common.obj->writeObj.ignorePicturePath;
            auto result = std::find_if(picturePathList.begin(), picturePathList.end(), [&filePath](const auto& path){
                return path == filePath;
            });
            if(result != picturePathList.end()){
                if(ignore == false){
                    picturePathList.erase(result);
                }
                else {
                    picturePathList.emplace_back(*result);
                }
            }
            else if(ignore){
                picturePathList.emplace_back(*result);
            }
        }
    });

    connect(this->ui->tableWidget_script, &QTableWidget::itemSelectionChanged, this, [this]()
    {
        auto selectItems = this->ui->tableWidget_script->selectedItems();
        if(selectItems.isEmpty()){ return; }
        auto item = selectItems[0];
        int row = item->row();
        auto text = this->ui->tableWidget_script->item(row, 3)->text();

        auto [fileName,textRow,textCol] = ::parseScriptNameWithRowCol(this->ui->tableWidget_script->item(row, 1)->text());
        auto scriptFilePath = this->common.obj->tempScriptFileDirectoryPath() + fileName;
        this->ui->scriptViewer->showFile(scriptFilePath);

        //スクリプトのスクロール
        this->ui->scriptViewer->scrollWithHighlight(textRow, textCol, text.length());
    });

    connect(this->ui->tableWidget_script, &QTableWidget::itemChanged, this, [this](QTableWidgetItem *item)
    {
        if(item->column() != 0){ return; }

        //無視リストの操作
        QStringList targetFileNames;
        const auto changeIgnoreState = [this, &targetFileNames](QTableWidgetItem *item, bool ignore)
        {
            auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
            auto filenameItem = this->ui->tableWidget_script->item(item->row(), 1);
            if(filenameItem == nullptr){ return; }
            auto [filename, row, col] = parseScriptNameWithRowCol(filenameItem->text());
            auto result = std::find_if(scriptList.begin(), scriptList.end(), [&filename](const auto& script){
                return script.name == filename;
            });
            if(targetFileNames.contains(filename) == false){ targetFileNames.append(filename); }

            std::pair<size_t, size_t> rcPair = {row, col};
            //MainComponentのinvokeAnalyzeと重複
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
            auto _item = this->ui->tableWidget_script->item(row, 0);
            changeIgnoreState(_item, ignore);
            _item->setCheckState(item->checkState());
        }
        this->ui->tableWidget_script->blockSignals(false);

        //ツリー側のチェック状態を変更
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

                auto checkState = Qt::Checked;
//                if(result != scriptList.end() && result->ignorePoint.empty() == false){
//                    checkState = Qt::PartiallyChecked;
//                }
                childItem->setCheckState(0, checkState);
                break;
            }
        }

    });

}

WriteModeComponent::~WriteModeComponent(){
}

void WriteModeComponent::show()
{
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

    int count = 0;
    for(const auto& info : languages)
    {
        auto [lang, flagName] = info;
        auto bcp47Name = lang.bcp47Name();
        if(bcp47Name == "zh"){ bcp47Name = "zh-cn"; }
        auto& langList = this->common.obj->languages;
        auto text = lang.nativeLanguageName() + "(" + bcp47Name.toLower() + ")";
        auto check = new QCheckBox(text);
        if(langList.contains(bcp47Name)){
            check->setChecked(true);
        }
        connect(check, &QCheckBox::toggled, this, [this, shortName = bcp47Name.toLower()](bool check)
        {
            auto& langList = this->common.obj->languages;
            if(check && langList.contains(shortName) == false)
            {
                langList.emplace_back(shortName);
            }
            else if(check==false && langList.contains(shortName)){
                langList.removeOne(shortName);
            }
        });
        this->ui->langTabGrid->addWidget(check, count/3, count%3);
        count++;
    }
    //通常データベース
    const auto translateFolder = this->common.obj->translateDirectoryPath();
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
        auto scriptFolder  = this->common.obj->tempScriptFileDirectoryPath();
        auto scriptFiles   = readCsv(scriptFolder + "_list.csv");
        auto scriptCsv     = readCsv(translateFolder + "Scripts.csv");
        auto writedScripts = [&scriptCsv](){
            QStringList result;
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

void WriteModeComponent::setNormalCsvText(QString fileName)
{
    const auto translateFolder = this->common.obj->translateDirectoryPath();
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
    const auto translateFolder = this->common.obj->translateDirectoryPath();
    auto csv = readCsv(translateFolder + "Scripts.csv");
    if(csv.empty()){ return; }

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

    const auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
    const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
            return x.name == fileName &&  std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
        });
        return result != scriptList.cend();
    };


    for(int r=0; r<csv.size()-1; ++r)
    {
        const auto& line = csv[r+1];

        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        targetTable->setItem(r, 0, checkItem);

        //名前
        {
            auto [fileName, row, col] = parseScriptNameWithRowCol(line[0]);
            auto* item = new QTableWidgetItem(line[0]);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            targetTable->setItem(r, 1, item);

            checkItem->setData(Qt::CheckStateRole, IsIgnoreText(fileName, row, col) ? Qt::Unchecked : Qt::Checked);
        }

        for(int c=1; c<line.size(); ++c){
            auto* item = new QTableWidgetItem(line[c]);
            item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            targetTable->setItem(r, c+1, item);
        }
    }

    targetTable->resizeColumnsToContents();
}
