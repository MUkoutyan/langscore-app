#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "LanguageSelectComponent.h"
#include "dialog/FirstWriteDialog.h"
#include "dialog/WriteDialog.h"
#include "dialog/UpdatePluginDialog.h"
#include "dialog/MessageDialog.h"
#include "ScriptViewer.h"
#include "ClipDetectSettingTree.h"
#include "src/invoker.h"
#include "../utility.hpp"
#include "../graphics.hpp"
#include "../csv.hpp"
#include "../service/EditingTranslationExporter.h"
#include <icu.h>

#include <QTimer>
#include <QDir>
#include <QString>
#include <QCheckBox>
#include <QLocale>
#include <QDirIterator>
#include <QDebug>
#include <QActionGroup>
#include <QMimeData>
#include <QMessageBox>
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
    , csvEditDataManager(std::make_shared<EditDataManager>())
{
    this->addDispatch(this);
    csvEditDataManager->ensureUndoStack();

    ui->setupUi(this);
    {
        this->scriptViewer = new ScriptViewer(this, this);
        auto index = this->ui->splitter->indexOf(this->ui->scriptViewer);
        this->ui->splitter->replaceWidget(index, this->scriptViewer);
        delete this->ui->scriptViewer;
        this->ui->scriptViewer = this->scriptViewer;

        this->fileTree = new FileTree(this, this->csvEditDataManager, this);
        this->addDispatch(this->fileTree);
        this->ui->splitter_2->insertWidget(0, this->fileTree);

        this->mainTable = new MainCSVTable(this, this->csvEditDataManager, this);
        this->addDispatch(this->mainTable);
        this->ui->verticalLayout->addWidget(this->mainTable, 1);

        this->scriptTable = new ScriptCSVTable(this, this->csvEditDataManager, this);
        this->addDispatch(this->scriptTable);
        this->ui->verticalLayout_8->addWidget(this->scriptTable, 1);

        //見切れ検出・制御文字の設定。ログの前に置く
        this->clipDetectSettingTree = new ClipDetectSettingTree(this->setting, this->history, this);
        this->ui->tabWidget->insertTab(this->ui->tabWidget->indexOf(this->ui->logTab), this->clipDetectSettingTree, tr("Text Clipping Detect"));
        //言語の有効・無効で列が変わるため、開くたびに作り直す（言語が同じなら何もしない）
        connect(this->ui->tabWidget, &QTabWidget::currentChanged, this, [this](int index) {
            if(this->ui->tabWidget->widget(index) == this->clipDetectSettingTree) {
                this->clipDetectSettingTree->setup();
            }
        });
    }

    {
        auto appSettings = ComponentBase::getAppSettings();
        if(appSettings.contains("WriteModeComponent/splitter")) {
            this->ui->splitter->restoreState(appSettings.value("WriteModeComponent/splitter").toByteArray());
        }
        if(appSettings.contains("WriteModeComponent/splitter_2")) {
            this->ui->splitter_2->restoreState(appSettings.value("WriteModeComponent/splitter_2").toByteArray());
        }
    }

    this->ui->logText->SetSettings(this->setting);
    this->setAcceptDrops(true);

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

        this->dispatch(EnableBlur, {});
        //this->setGraphicsEffect(new QGraphicsBlurEffect);
        this->changeEnabledUIState(false);
        auto result = mes.exec();
        this->dispatch(DisableBlur, {});
        //this->setGraphicsEffect(nullptr);
        if(result != QDialog::Accepted){
            this->changeEnabledUIState(true);
            return;
        }

        this->ui->tabWidget->setCurrentWidget(this->ui->logTab);
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
    connect(_invoker, &invoker::finish, this, [this](int exitCode, QProcess::ExitStatus status)
    {
        this->changeEnabledUIState(true);
        if(exitCode != invoker::SUCCESS || status == QProcess::CrashExit){
            //失敗した処理を完了扱いにしない（初回書き出し済みフラグの保存なども行わない）
            this->ui->logText->insertPlainText(tr("Failed."));
            this->dispatch(StatusMessage, {tr("Failed. Please check the log."), 5000});
            //パッキングの前に CSV とプラグインの書き出しは終わっているため、書き出し済みとして扱う。
            //パッキングは「翻訳CSVを書き出し」でやり直せる
            if(invokeType == InvokeType::PackingAfterExport) {
                //パッキングに失敗しました。CSVは書き出されていますが、ゲームでは以前の翻訳が使われる場合があります。
                //原因を取り除いてから「翻訳CSVを書き出し」をもう一度実行してください。
                this->ui->logText->writeText("\n" + tr("Packing failed. The CSV files have been exported, but the game may still use the previous translations.\nFix the cause and run \"Write Translate CSV\" again.") + "\n");
                this->finishExport(false);
            }
            invokeType = InvokeType::None;
            return;
        }
        this->ui->logText->insertPlainText(tr("Done."));

        if(invokeType == InvokeType::Reanalisys){
            this->clear();
            //解析フォルダの内容が変わったため、ファイル一覧などを読み直す
            this->setting->load(this->setting->langscoreProjectDirectory + "/config.json");
            this->setup();
            this->dispatch(StatusMessage, {tr(" Complete."), 3000});
        }
        else if(invokeType == InvokeType::ExportFirstTime || invokeType == InvokeType::ExportCSV){
            this->applyEditingTranslationsToExport();
            //VXAce は翻訳を反映したCSVをパッキングしないとゲームで使えない
            if(this->setting->projectType == settings::VXAce) {
                this->changeEnabledUIState(false);
                invokeType = InvokeType::PackingAfterExport;
                this->_invoker->packing();
                return;
            }
            this->finishExport();
        }
        else if(invokeType == InvokeType::PackingAfterExport){
            //ゲームは .rvdata2 を読むため、パッキング済みのCSVは残さない
            const auto removedFiles = langscore::RemovePackedCsvFiles(*this->setting);
            this->ui->logText->writeText("\n" + tr("Removed %1 packed CSV files.").arg(removedFiles.size()) + "\n");
            this->finishExport();
        }
        invokeType = InvokeType::None;
    });


#ifdef QT_DEBUG
    auto* configView = new QPlainTextEdit(this);
    configView->setObjectName("Config View");
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
    connect(fileTree, &FileTree::scriptTableScrollToRow, this->scriptTable, &ScriptCSVTable::onScriptTableScrollToRow);
    connect(fileTree, &FileTree::scriptTableSelectRow, this->scriptTable, &ScriptCSVTable::onScriptTableSelectRow);
    connect(fileTree, &FileTree::showGraphicsImage, this, &WriteModeComponent::onShowGraphicsImage);
    connect(fileTree, &FileTree::showMainFile, this, &WriteModeComponent::onShowMainFile);
    connect(fileTree, &FileTree::selectMainError, this->mainTable, &MainCSVTable::selectError);
    connect(fileTree, &FileTree::selectScriptError, this->scriptTable, &ScriptCSVTable::selectRowByOriginal);
    connect(this->mainTable, &MainCSVTable::fileProgressChanged, fileTree, &FileTree::setFileProgress);
connect(fileTree, &FileTree::showScriptFile, this, &WriteModeComponent::onShowScriptFile);
    connect(fileTree, &FileTree::setScriptFileNameLabel, this, &WriteModeComponent::onSetScriptFileNameLabel);
    connect(fileTree, &FileTree::highlightScriptText, this, &WriteModeComponent::onHighlightScriptText);
    connect(fileTree, &FileTree::setTabIndex, this, &WriteModeComponent::onSetTabIndex);

    connect(fileTree, &FileTree::notifyScriptTreeItemCheckChanged, this, &WriteModeComponent::onScriptTableItemCheckChanged);
    connect(scriptTable, &ScriptCSVTable::scriptTableSelected, this, &WriteModeComponent::scriptTableSelected);
    connect(scriptTable, &ScriptCSVTable::notifyScriptTableChangeItemCheck, this, &WriteModeComponent::onScriptTreeItemCheckChanged);

    connect(this->ui->splitter, &QSplitter::splitterMoved, this, [this](int, int) {
        auto settings = ComponentBase::getAppSettings();
        settings.setValue("WriteModeComponent/splitter", this->ui->splitter->saveState());
    });
    connect(this->ui->splitter_2, &QSplitter::splitterMoved, this, [this](int, int) {
        auto settings = ComponentBase::getAppSettings();
        settings.setValue("WriteModeComponent/splitter_2", this->ui->splitter_2->saveState());
    });
}

WriteModeComponent::~WriteModeComponent(){
    this->removeDispatch(this);
}

void WriteModeComponent::show()
{
    //OS の言語は新規プロジェクトの既定言語として setupLanguages で有効になる。開くたびには変えない
    this->clear();
    this->setup();
}

void WriteModeComponent::clear()
{

    this->showAllScriptContents = true;
    //ツリー・表を作り直すと Undo 履歴が保持している項目が削除されるため、履歴も破棄する
    this->history->clear();
    this->fileTree->clear();
    this->mainTable->clear();
    this->scriptTable->clear();
    //再解析・プロジェクト切り替え後は保持している編集データが古くなるため破棄する（保存は SaveProject で済んでいる）
    this->csvEditDataManager->closeAllSessions();
    this->scriptViewer->clear();
    for(auto langButton : this->languageButtons){
        this->ui->langTabGrid->removeWidget(langButton);
        delete langButton;
    }
    this->languageButtons.clear();

    this->update();
}

//全検証の前に、まだ開いていないファイルもアプリ側エディターに保存されるよう読み込む
void WriteModeComponent::loadAllMainFiles()
{
    this->mainTable->loadAllMainFiles();
}

QString WriteModeComponent::importTranslationCsvFiles(const QStringList& csvPaths)
{
    int changedCells = 0;
    QStringList skippedFiles;
    for(const auto& csvPath : csvPaths)
    {
        auto result = this->scriptTable->importTranslationCsv(csvPath);
        if(result < 0) { result = this->mainTable->importTranslationCsv(csvPath); }
        if(result < 0)
        {
            skippedFiles.append(QFileInfo(csvPath).fileName());
            continue;
        }
        changedCells += result;
    }
    this->mainTable->notifyAllFileProgress();

    auto message = tr("Imported %1 translations.").arg(changedCells);
    if(0 < changedCells) {
        message += "\n" + tr("Save the project to keep the imported translations.");
    }
    if(skippedFiles.isEmpty() == false) {
        message += "\n\n" + tr("The following files were skipped because no matching file was found in the project:") + "\n" + skippedFiles.join("\n");
    }
    return message;
}

void WriteModeComponent::treeItemSelected()
{
    this->fileTree->itemSelected();
}

void WriteModeComponent::treeItemChanged(QTreeWidgetItem *_item, int column)
{
    this->fileTree->itemChanged(_item, column);
}

void WriteModeComponent::scriptTableSelected(QString scriptName, QString fileName, QString textPoint, int textLen)
{
    auto scriptFilePath = this->setting->tempScriptFileDirectoryPath() + "/" + fileName + GetScriptExtension(this->setting->projectType);
    this->scriptViewer->showFile(scriptFilePath);

    constexpr size_t invalid = std::numeric_limits<size_t>::max();
    auto [textRow, textCol] = parseScriptWithRowCol(textPoint);

    if(textRow == invalid && textCol == invalid) {
        this->scriptViewer->updateSearchHighlights("@default " + textPoint);
    }
    else if(textRow != std::numeric_limits<size_t>::max())
    {
        this->scriptViewer->scrollWithHighlight(textRow, textCol, textLen);
    }

}

void WriteModeComponent::exportPlugin()
{
    if(this->setting->writeObj.exportDirectory.isEmpty()){
        this->setting->writeObj.exportDirectory = this->setting->translateDirectoryPath();
    }
    this->dispatch(EnableBlur, {});
    //this->setGraphicsEffect(new QGraphicsBlurEffect);
    this->changeEnabledUIState(false);

    //ダイアログはチェックのたびに設定を書き換えるため、中止したときに戻せるよう控えておく
    const auto prevWriteObj = this->setting->writeObj;
    UpdatePluginDialog dialog(this->setting, this);

    auto execCode = dialog.exec();
    this->dispatch(DisableBlur, {});
    //this->setGraphicsEffect(nullptr);
    if(execCode == QDialog::Rejected){
        this->setting->writeObj = prevWriteObj;
        this->changeEnabledUIState(true);
        return;
    }

    //パッチモードを切り替えるとCSVの置き場所が変わるため、CSVも書き出し直す
    const bool patchModeChanged = prevWriteObj.enableLanguagePatch != dialog.isEnableLanguagePatch();
    if(patchModeChanged) { this->setting->writeObj.overwriteLangscore = true; }

    if(this->confirmOverwriteReadOnlyPlugins() == false) {
        this->setting->writeObj = prevWriteObj;
        this->changeEnabledUIState(true);
        return;
    }

    //アプリ再起動後など、このセッションで書き出しをしていない場合は保存済みの出力先をそのまま使う
    if(lastWritePath.isEmpty() == false) {
        QDir lsProjDir(this->setting->langscoreProjectDirectory);
        this->setting->writeObj.exportDirectory = lsProjDir.relativeFilePath(lastWritePath);
    }
    this->setting->writeObj.enableLanguagePatch = dialog.isEnableLanguagePatch();
    this->setting->setPackingDirectory(this->setting->writeObj.exportDirectory);

    this->ui->tabWidget->setCurrentWidget(this->ui->logTab);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Update Plugin...\n"));

    this->dispatch(SaveProject,{});
    if(patchModeChanged) {
        //CSV の書き出し後は通常の書き出しと同じく、編集内容の反映とパッキングを行う
        _invoker->exportFirstTime();
        invokeType = InvokeType::ExportCSV;
        return;
    }
    _invoker->updatePlugin();
    invokeType = InvokeType::UpdatePlugin;
}

void WriteModeComponent::exportTranslateFiles()
{
    if(this->setting->writeObj.exportDirectory.isEmpty()){
        this->setting->writeObj.exportDirectory = this->setting->translateDirectoryPath();
    }
    this->dispatch(EnableBlur, {});
    //this->setGraphicsEffect(new QGraphicsBlurEffect);
    this->changeEnabledUIState(false);

    WriteDialog dialog(this->setting, this);

    auto execCode = dialog.exec();
    this->dispatch(DisableBlur, {});
    //this->setGraphicsEffect(nullptr);
    if(execCode == QDialog::Rejected){
        this->changeEnabledUIState(true);
        return;
    }

    //パッチモードを切り替えると、プラグインも更新しないとゲームが新しい置き場所のCSVを読まない。
    //VXAce のパッチモードの設定は langscore.rb に書き込まれるため、langscore.rb を上書きする
    const bool patchModeChanged = this->setting->writeObj.enableLanguagePatch != dialog.writeByLanguagePatch();
    if(patchModeChanged)
    {
        const bool prevOverwriteLangscore = this->setting->writeObj.overwriteLangscore;
        this->setting->writeObj.overwriteLangscore = true;
        if(this->confirmOverwriteReadOnlyPlugins() == false) {
            this->setting->writeObj.overwriteLangscore = prevOverwriteLangscore;
            this->changeEnabledUIState(true);
            return;
        }
    }

    QDir lsProjDir(this->setting->langscoreProjectDirectory);
    lastWritePath = dialog.outputPath();
    auto relativePath = lsProjDir.relativeFilePath(lastWritePath);
    this->setting->writeObj.exportDirectory = relativePath;
    this->setting->writeObj.enableLanguagePatch = dialog.writeByLanguagePatch();
    this->setting->writeObj.writeMode = dialog.writeMode();
    this->setting->setPackingDirectory(relativePath);

    if(dialog.backup()){ backup(); }

    this->ui->tabWidget->setCurrentWidget(this->ui->logTab);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));

    this->dispatch(SaveProject,{});
    if(patchModeChanged) { _invoker->exportFirstTime(); }
    else { _invoker->exportCSV(); }
    invokeType = InvokeType::ExportCSV;

}

void WriteModeComponent::firstExportTranslateFiles()
{
    if(this->setting->writeObj.exportDirectory.isEmpty()){
        this->setting->writeObj.exportDirectory = this->setting->translateDirectoryPath();
    }
    this->dispatch(EnableBlur, {});
    //this->setGraphicsEffect(new QGraphicsBlurEffect);
    this->changeEnabledUIState(false);

    FirstWriteDialog dialog(this->setting, this);

    auto execCode = dialog.exec();
    this->dispatch(DisableBlur, {});
    //this->setGraphicsEffect(nullptr);
    if(execCode == QDialog::Rejected){
        this->changeEnabledUIState(true);
        return;
    }

    if(this->confirmOverwriteReadOnlyPlugins() == false) {
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

    this->ui->tabWidget->setCurrentWidget(this->ui->logTab);

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files(First)...\n"));

    this->dispatch(SaveProject,{});
    _invoker->exportFirstTime();
    invokeType = InvokeType::ExportFirstTime;
}

void WriteModeComponent::setup()
{

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
            //開いている表に、有効にした言語の列を足す
            this->mainTable->refreshLanguageColumns();
            this->scriptTable->refreshLanguageColumns();
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
    this->mainTable->notifyAllFileProgress();
    this->scriptTable->setupScriptTable();
    //設定を読み直した後なので、古い設定を指している項目を残さないよう必ず作り直す
    this->clipDetectSettingTree->setup(true);

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

//divisiでアプリ側エディターのCSVを認識するため、エディター内の翻訳をCSVに反映する
void WriteModeComponent::applyEditingTranslationsToExport()
{
    const auto updatedFiles = langscore::ApplyEditingTranslations(*this->setting);
    //編集した翻訳を %1 個のファイルに反映しました。
    this->ui->logText->writeText("\n" + tr("Applied the edited translations to %1 files.").arg(updatedFiles.size()) + "\n");
}

void WriteModeComponent::finishExport(bool openExportFolder)
{
    if(openExportFolder) {
        QProcess::startDetached("explorer", {QDir::toNativeSeparators(lastWritePath)});
    }
    this->setting->isFirstExported = true;

    this->ui->writeCSVAndPluginButton->setVisible(false);
    this->ui->updatePluginButton->setVisible(true);
    this->ui->writeButton->setVisible(true);

    this->dispatch(SaveProject,{});
}

//上書きされる読み取り専用のプラグインがあれば、上書きしてよいかを確認する。
//中止した場合は false を返す
bool WriteModeComponent::confirmOverwriteReadOnlyPlugins()
{
    if(this->setting->writeObj.overwriteLangscore == false) { return true; }

    //VXAce のプラグインは Data/Scripts.rvdata2 に書き込まれる
    const QStringList pluginFiles = this->setting->projectType == settings::VXAce
        ? QStringList{"Data/Scripts.rvdata2"}
        : QStringList{"js/plugins/Langscore.js", "js/plugins/Langscore_ObserverBridge.js"};
    QStringList readOnlyFiles;
    for(const auto& name : pluginFiles) {
        QFileInfo info(this->setting->gameProjectPath + "/" + name);
        if(info.exists() && info.isWritable() == false) {
            readOnlyFiles.append(QDir::toNativeSeparators(info.absoluteFilePath()));
        }
    }
    if(readOnlyFiles.isEmpty()) { return true; }

    //次のファイルは読み取り専用です。上書きしますか？
    const auto answer = QMessageBox::question(this, tr("Overwrite read-only files"),
        tr("The following files are read-only. Do you want to overwrite them?") + "\n\n" + readOnlyFiles.join("\n"));
    return answer == QMessageBox::Yes;
}



void WriteModeComponent::changeUIColor()
{
    this->fileTree->updateTreeTextColor();

    this->scriptTable->updateTableTextColor();


}

void WriteModeComponent::setFontList(std::vector<QString> fontPaths)
{
    auto lsProjDir = QDir(this->setting->langscoreProjectDirectory);
    lsProjDir.mkpath("Fonts"); 
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
    this->ui->writeCSVAndPluginButton->setEnabled(enable);
    this->ui->updatePluginButton->setEnabled(enable);
    this->ui->updateButton->setEnabled(enable);
    //言語が1つも選ばれていない場合は、処理が終わっても書き出しボタンを無効のままにする
    const bool hasEnabledLanguage = std::ranges::any_of(this->setting->languages, [](const settings::Language& lang) {
        return lang.enable;
    });
    this->ui->writeButton->setEnabled(enable && hasEnabledLanguage);
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
    else if(type == ComponentBase::SaveProject)
    {
        const auto editFolder = this->setting->langscoreProjectDirectory + "/editing";
        this->csvEditDataManager->saveAllModels(editFolder);
    }
    else if(type == ComponentBase::EnableBlur) {
        this->setGraphicsEffect(new QGraphicsBlurEffect);
    }
    else if(type == ComponentBase::DisableBlur) {
        this->setGraphicsEffect(nullptr);
    }
    else if(type == ComponentBase::SendLog) 
    {
        if(args.empty()) { return; }
        auto text = args[0].toString();
        this->ui->logText->writeText(text);
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

    QString currentDate = QDate::currentDate().toString("yyyy-MM-dd") + "-" + QTime::currentTime().toString("HHmm");
    QString backupFolderPath = this->setting->langscoreProjectDirectory+"/backup/"+currentDate;
    QDir().mkpath(backupFolderPath);

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
    QImage image(imagePath);
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
    this->fileTree->onChangeScriptTableItemCheck(scriptName, check);
}

void WriteModeComponent::onScriptTableItemCheckChanged(QString scriptName, Qt::CheckState check)
{
    this->scriptTable->changeScriptTableItemCheck(scriptName, check);
}