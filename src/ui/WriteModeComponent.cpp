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

#include "FileTree.h"
#include "ScriptCSVTable.h"

using namespace langscore;


namespace {


} //namespace {

WriteModeComponent::WriteModeComponent(ComponentBase* setting, QWidget* parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::WriteModeComponent)
    , scene(new QGraphicsScene(this))
    , languageButtons()
    , _invoker(new invoker(this))
    , invokeType(InvokeType::None)
    , lastWritePath("")
    , scriptViewer(nullptr)
    , loadFileManager(std::make_shared<LoadFileManager>())
{

    this->addDispatch(this);

    ui->setupUi(this);
    {
        this->scriptViewer = new ScriptViewer(this, this);
        auto index = this->ui->splitter->indexOf(this->ui->scriptViewer);
        this->ui->splitter->replaceWidget(index, this->scriptViewer);
        delete this->ui->scriptViewer;
        this->ui->scriptViewer = this->scriptViewer;

        this->fileTree = new FileTree(this, this->loadFileManager, this);
        this->ui->splitter_2->insertWidget(0, this->fileTree);

        this->mainTable = new MainCSVTable(this, this->loadFileManager, this);
        this->ui->verticalLayout->addWidget(this->mainTable, 1);

        this->scriptTable = new ScriptCSVTable(this, this->loadFileManager, this);
        this->ui->verticalLayout_8->addWidget(this->scriptTable, 1);
    }

    this->ui->modeDesc->hide();
    this->ui->logText->SetSettings(this->setting);
    this->setAcceptDrops(true);

    connect(this->ui->toPackingMode, &QToolButton::clicked, this, &WriteModeComponent::toNextPage);

    connect(this->ui->writeCSVAndPluginButton, &QPushButton::clicked, this, &WriteModeComponent::firstExportTranslateFiles);
    connect(this->ui->writeButton, &QPushButton::clicked, this, &WriteModeComponent::exportTranslateFiles);
    connect(this->ui->updatePluginButton, &QPushButton::clicked, this, &WriteModeComponent::exportPlugin);

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

    this->fileTree->setStyleSheet(R"(
QTreeView::item {
    height: 28px;
})");
    this->ui->graphicsView->setStyleSheet("QGraphicsView{background : #858585;}");


    // --- signal/slot接続 ---
    //connect(scriptTreeManager, &FileTree::scriptTableScrollToRow, this, &WriteModeComponent::onScriptTableScrollToRow);
    //connect(scriptTreeManager, &FileTree::scriptTableSelectRow, this, &WriteModeComponent::onScriptTableSelectRow);
    connect(fileTree, &FileTree::showGraphicsImage, this, &WriteModeComponent::onShowGraphicsImage);
    connect(fileTree, &FileTree::showMainFile, this, &WriteModeComponent::onShowMainFile);
    connect(fileTree, &FileTree::showScriptFile, this, &WriteModeComponent::onShowScriptFile);
    connect(fileTree, &FileTree::setScriptFileNameLabel, this, &WriteModeComponent::onSetScriptFileNameLabel);
    connect(fileTree, &FileTree::highlightScriptText, this, &WriteModeComponent::onHighlightScriptText);
    connect(fileTree, &FileTree::setTabIndex, this, &WriteModeComponent::onSetTabIndex);

    connect(fileTree, &FileTree::scriptTreeItemCheckChanged, this, &WriteModeComponent::onScriptTreeItemCheckChanged);
    //connect(this, &WriteModeComponent::changeScriptTableItemCheck, scriptTableManager, &ScriptCSVTable::changeScriptTableItemCheck);
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

    auto writedScriptList = [&analyzeDirPath]()
    {
        QStringList result;
        auto scriptCsv     = readCsv(analyzeDirPath + "/Scripts.csv");
        if(scriptCsv.empty()){ return result; }
        scriptCsv.erase(scriptCsv.begin()); //Headerを削除
        for(auto& line : scriptCsv){
            result.emplace_back(line[0]);
        }
        result.sort();
        result.erase(std::unique(result.begin(), result.end()), result.end());

        return result;
    }();

    for(auto& script : writedScriptList)
    {
        auto parseResult = parseScriptNameWithRowCol(script);
        QString fileName;
        size_t row = 0;
        size_t col = 0;
        if(std::holds_alternative<TextPosition::RowCol>(parseResult.d)){
            fileName = parseResult.scriptFileName;
            const auto& cell = std::get<TextPosition::RowCol>(parseResult.d);
            row = cell.row;
            col = cell.col;
        }
        else{
            continue;
        }

        auto& scriptList = this->setting->writeObj.scriptInfo;
        const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
                return x.name == fileName &&  std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
            });
            return result != scriptList.cend();
        };
        if(IsIgnoreText(fileName, row, col) == false){ continue; }

        auto& info = this->setting->fetchScriptInfo(fileName);
        if(std::find_if(info.ignorePoint.begin(), info.ignorePoint.end(), [row, col](const auto& x){
            return x == std::pair<size_t,size_t>{row, col};
        }) == info.ignorePoint.end())
        {
            settings::TextPoint point;
            point.row = row;
            point.col = col;
            info.ignorePoint.emplace_back(std::move(point));
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
    this->ui->mainFileName->setText("");
    this->ui->mainFileWordCount->setText("");

    this->showAllScriptContents = true;
    this->fileTree->clear();
    this->scriptTable->clear();
    this->scriptViewer->clear();
    for(auto langButton : this->languageButtons){
        this->ui->langTabGrid->removeWidget(langButton);
        delete langButton;
    }
    this->languageButtons.clear();

    this->update();
}

void WriteModeComponent::treeItemSelected()
{
    this->fileTree->itemSelected();
}

void WriteModeComponent::treeItemChanged(QTreeWidgetItem *_item, int column)
{
    this->fileTree->itemChanged(_item, column);
}

void WriteModeComponent::scriptTableSelected(QString scriptName, QString fileName, size_t textRow, size_t textCol, int textLen)
{
    auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + fileName + GetScriptExtension(this->setting->projectType);


    this->scriptViewer->showFile(scriptFilePath);
    if(textRow != std::numeric_limits<size_t>::max()){
        this->scriptViewer->scrollWithHighlight(textRow, textCol, textLen);
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
    this->loadFileManager->loadFile(this->setting);

    this->ui->tabWidget->setCurrentIndex(0);
    //this->ui->tableWidget_script->blockSignals(true);
    this->fileTree->blockSignals(true);

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

    this->fileTree->setupTree();
    this->scriptTable->setupScriptTable();

    this->changeUIColor();

    this->setting->saveForProject();

    //this->ui->tableWidget_script->blockSignals(false);
    this->fileTree->blockSignals(false);

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


void WriteModeComponent::changeUIColor()
{
    this->fileTree->updateTreeTextColor();

    this->scriptTable->updateTableTextColor();


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
    this->fileTree->setEnabled(enable);
    //this->ui->tableWidget_script->setEnabled(enable);
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

void WriteModeComponent::onShowGraphicsImage(const QString& imagePath)
{
    scene->clear();
    this->ui->graphicsView->verticalScrollBar()->setValue(0);
    this->ui->graphicsView->horizontalScrollBar()->setValue(0);
    QDir graphFolder(this->setting->tempGraphicsFileDirectoryPath());
    auto absImagePath = graphFolder.absoluteFilePath(imagePath);
    QImage image(absImagePath);
    auto* imageItem = new QGraphicsPixmapItem(QPixmap::fromImage(image));
    scene->addItem(imageItem);
    this->ui->graphicsView->setScene(scene);
}

void WriteModeComponent::onShowMainFile(const QString& treeItemName, const QString& fileName)
{
    this->mainTable->showMainFileText(treeItemName, fileName);
}

void WriteModeComponent::onShowScriptFile(const QString& scriptFilePath)
{
    this->scriptViewer->showFile(scriptFilePath);
}

void WriteModeComponent::onSetScriptFileNameLabel(const QString& label)
{
    this->scriptTable->setScriptFileName(label);
}

void WriteModeComponent::onHighlightScriptText(int row, int col, int length)
{
    this->scriptViewer->scrollWithHighlight(row, col, length);
}

void WriteModeComponent::onSetTabIndex(int index)
{
    this->ui->tabWidget->setCurrentIndex(index);
}

void WriteModeComponent::onScriptTreeItemCheckChanged(QString scriptName, Qt::CheckState check)
{
    if(scriptTable) {
        scriptTable->changeScriptTableItemCheck(scriptName, check);
    }
}