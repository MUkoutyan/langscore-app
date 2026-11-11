#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "LanguageSelectComponent.h"
#include "dialog/WriteDialog.h"
#include "dialog/UpdatePluginDialog.h"
#include "dialog/MessageDialog.h"
#include "dialog/RPGMakerPathDialog.h"
#include "ScriptViewer.h"
#include "ClipDetectSettingTree.h"
#include "src/invoker.h"
#include "../utility.hpp"
#include "../graphics.hpp"
#include "../csv.hpp"
#include "../service/EditingTranslationExporter.h"
#include "MainCSVTableModel.h"
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
#include <QUndoStack>
#include <QCryptographicHash>

#include <functional>
#include <unordered_map>

#include "FileTree.h"
#include "ScriptCSVTable.h"

using namespace langscore;


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
        this->ui->tabWidget->insertTab(this->ui->tabWidget->indexOf(this->ui->logTab), this->clipDetectSettingTree, tr("Text Clipping Detection"));
        //言語の有効・無効で列が変わるため、開くたびに再構築する。言語が同じ場合は何もしない
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

    //プラグインの追加も、プラグインの更新と同じダイアログで行う
    connect(this->ui->addPluginButton, &QPushButton::clicked, this, &WriteModeComponent::exportPlugin);
    connect(this->ui->writeButton, &QPushButton::clicked, this, &WriteModeComponent::exportTranslateFiles);
    connect(this->ui->updatePluginButton, &QPushButton::clicked, this, &WriteModeComponent::exportPlugin);
    connect(this->ui->testPlayButton, &QPushButton::clicked, this, &WriteModeComponent::testPlay);
    this->setupRemoteHub();

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

        this->showLogTab();
        this->ui->logText->clear();
        this->ui->logText->insertPlainText(tr("Updating project...\n"));

        auto baseMessage = tr("Updating to the latest content...");
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
            //失敗した処理を完了扱いにせず、初回書き出し済みフラグの保存なども行わない
            this->ui->logText->insertPlainText(tr("Failed."));
            this->dispatch(StatusMessage, {tr("Failed. Please check the log."), 5000});
            //書き出しに失敗した場合、古い翻訳文のままテストプレイしない
            testPlayAfterExport = false;
            applyToGameAfterExport = false;
            //ログを表示したままにする。次の処理が成功したときに古いタブへ戻らないよう、記録も消す
            this->tabBeforeLog = nullptr;
            //パッキングの前に CSV とプラグインの書き出しは終わっているため、書き出し済みとして扱う。
            //パッキングは「翻訳CSVを出力」でやり直せる
            if(invokeType == InvokeType::PackingAfterExport) {
                //パッキングに失敗しました。CSVは書き出されていますが、ゲームでは以前の翻訳が使われる場合があります。
                //原因を取り除いてから「翻訳CSVを出力」をもう一度実行してください。
                this->ui->logText->writeText("\n" + tr("Packing failed. The CSV files have been exported, but the game may still use the previous translations.\nFix the cause and run \"Export Translate CSV\" again.") + "\n");
                this->finishExport(false);
            }
            invokeType = InvokeType::None;
            return;
        }
        this->ui->logText->insertPlainText(tr("Done."));

        if(invokeType == InvokeType::Reanalisys){
            this->clear();
            //解析フォルダの内容が変更されたため、ファイル一覧などを読み直す
            this->setting->load(this->setting->langscoreProjectDirectory + "/config.json");
            this->setup();
            this->dispatch(StatusMessage, {tr(" Complete."), 3000});
        }
        else if(invokeType == InvokeType::UpdatePlugin){
            this->setting->isPluginRegistered = true;
            this->updateExportButtonsVisible();
            this->dispatch(SaveProject, {});
        }
        else if(invokeType == InvokeType::ExportCSV){
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
        //問題が無ければ、ログを表示する前のタブに戻す。失敗した場合はログを表示したままにする
        this->restoreTabBeforeLog();
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
    connect(this->mainTable, &MainCSVTable::fileMarksChanged, fileTree, &FileTree::setFileMarks);
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
    this->saveViewState();
    this->removeDispatch(this);
}

void WriteModeComponent::show()
{
    //OS の言語は新規プロジェクトの既定言語として setupLanguages で有効になる。開くたびには変更しない
    this->clear();
    this->setup();
    //前回ゲームに反映していない編集があれば、反映できるようにする
    this->setHasUnappliedEdits(this->setting->isFirstExported && langscore::UnexportedEditingFiles(*this->setting).isEmpty() == false);
}

void WriteModeComponent::clear()
{
    //表を消す前に、表示していたプロジェクトの表示の状態を保存する
    this->saveViewState();
    this->viewStateGroup.clear();

    this->showAllScriptContents = true;
    //別のプロジェクトのゲームを監視しないよう、追従を止める
    if(this->followGameButton) { this->followGameButton->setChecked(false); }
    //ツリー・表を再構築すると Undo 履歴が保持している項目が削除されるため、履歴も破棄する
    this->history->clear();
    this->fileTree->clear();
    this->mainTable->clear();
    this->scriptTable->clear();
    //再解析・プロジェクト切り替え後は保持している編集データが古くなるため破棄する。保存は SaveProject で完了済
    this->csvEditDataManager->closeAllSessions();
    this->scriptViewer->clear();
    for(auto langButton : this->languageButtons){
        this->ui->langTabGrid->removeWidget(langButton);
        delete langButton;
    }
    this->languageButtons.clear();

    this->update();
}

//全検証の前に、未表示のファイルもアプリ側エディターに保存されるよう読み込む
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

    //ダイアログはチェックのたびに設定を書き換えるため、中止時に復元できるよう保持する
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

    //パッチモードを切り替えるとCSVの置き場所が変わるため、書き出し済みのCSVも書き出し直す。
    //CSV の書き出し前にプラグインを追加する場合は、プラグインのみ書き出す
    const bool patchModeChanged = prevWriteObj.enableLanguagePatch != dialog.isEnableLanguagePatch();
    const bool reexportCsv = patchModeChanged && this->setting->isFirstExported;
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

    this->showLogTab();

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Update Plugin...\n"));

    this->dispatch(SaveProject,{});
    if(reexportCsv) {
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

    QDir lsProjDir(this->setting->langscoreProjectDirectory);
    lastWritePath = dialog.outputPath();
    auto relativePath = lsProjDir.relativeFilePath(lastWritePath);
    this->setting->writeObj.exportDirectory = relativePath;
    //言語パッチモードは、プラグインを書き出したときの設定に合わせる
    this->setting->writeObj.writeMode = dialog.writeMode();
    this->setting->setPackingDirectory(relativePath);

    if(dialog.backup()){ backup(); }

    this->showLogTab();

    this->ui->logText->clear();
    this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));

    this->dispatch(SaveProject,{});
    _invoker->exportCSV();
    invokeType = InvokeType::ExportCSV;

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
    const auto languages = settings::supportedLocales();
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
            //開いている表に、有効にした言語の列を追加する
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
                this->ui->writeButton->setText(QCoreApplication::translate("WriteModeComponent", "Export Translate CSV", nullptr));
            }
        });
        languageButtons.emplace_back(langComp);
        count++;
    }

    this->fileTree->setupTree();
    this->mainTable->notifyAllFileProgress();
    this->scriptTable->setupScriptTable();
    //設定の読み直し後のため、古い設定を指す項目を残さないよう必ず再構築する
    this->clipDetectSettingTree->setup(true);

    this->changeUIColor();

    this->setting->saveForProject();

    //this->ui->tableWidget_script->blockSignals(false);
    this->fileTree->blockSignals(false);

    this->updateExportButtonsVisible();

    this->viewStateGroup = "WriteModeComponent/view_" + QString::fromLatin1(QCryptographicHash::hash(this->setting->langscoreProjectDirectory.toUtf8(), QCryptographicHash::Md5).toHex());
    this->restoreViewState();

    this->update();
}

//処理のログを表示する。表示する前のタブは restoreTabBeforeLog で戻せるよう記録する
void WriteModeComponent::showLogTab()
{
    if(this->ui->tabWidget->currentWidget() != this->ui->logTab) {
        this->tabBeforeLog = this->ui->tabWidget->currentWidget();
    }
    this->ui->tabWidget->setCurrentWidget(this->ui->logTab);
}

void WriteModeComponent::restoreTabBeforeLog()
{
    //再解析では setup がタブを切り替えるため、表示中のタブによらず戻す
    if(this->tabBeforeLog && this->ui->tabWidget->indexOf(this->tabBeforeLog) != -1) {
        this->ui->tabWidget->setCurrentWidget(this->tabBeforeLog);
    }
    this->tabBeforeLog = nullptr;
}

void WriteModeComponent::saveViewState()
{
    if(this->viewStateGroup.isEmpty()) { return; }
    auto settings = ComponentBase::getAppSettings();
    settings.beginGroup(this->viewStateGroup);
    settings.setValue("file", this->fileTree->currentFile());
    //書き出しなどで自動的に切り替わるログのタブは、開き直したときに表示しない
    if(this->ui->tabWidget->currentWidget() != this->ui->logTab) {
        settings.setValue("tab", this->ui->tabWidget->currentIndex());
    }
    const auto position = this->mainTable->viewPosition();
    settings.setValue("row", position.row);
    settings.setValue("column", position.column);
    settings.setValue("scroll", position.scroll);
    settings.endGroup();
}

//初めて開いたプロジェクトは保存した状態が無いため、言語のタブのまま
void WriteModeComponent::restoreViewState()
{
    auto settings = ComponentBase::getAppSettings();
    settings.beginGroup(this->viewStateGroup);
    //ファイルを選択するとそのファイルのタブに切り替わるため、タブはファイルの後に復元する
    const auto file = settings.value("file").toString();
    if(file.isEmpty() == false && this->fileTree->selectFile(file)) {
        MainCSVTable::ViewPosition position;
        position.row = settings.value("row", -1).toInt();
        position.column = settings.value("column", -1).toInt();
        position.scroll = settings.value("scroll", 0).toInt();
        this->mainTable->restoreViewPosition(position);
    }
    const auto tab = settings.value("tab", 0).toInt();
    if(0 <= tab && tab < this->ui->tabWidget->count()) {
        this->ui->tabWidget->setCurrentIndex(tab);
    }
    settings.endGroup();
}



void WriteModeComponent::setupRemoteHub()
{
    //ゲームに追従
    this->followGameButton = new QPushButton(tr("Follow Game"), this);
    this->followGameButton->setObjectName("followGameButton");
    this->followGameButton->setCheckable(true);
    //ゲームへ反映
    this->applyToGameButton = new QPushButton(tr("Apply to Game"), this);
    this->applyToGameButton->setObjectName("applyToGameButton");
    //編集した翻訳をゲームの翻訳ファイルに反映し、プロジェクトに保存します。テストプレイ中のゲームは翻訳を読み直します。
    this->applyToGameButton->setToolTip(tr("Applies the edited translations to the game's translation files and saves the project.\nThe running game reloads the translations."));
    for(auto* button : {this->followGameButton, this->applyToGameButton}) {
        button->setSizePolicy(this->ui->testPlayButton->sizePolicy());
        button->setMinimumSize(this->ui->testPlayButton->minimumSize());
        button->setMaximumSize(this->ui->testPlayButton->maximumSize());
    }
    this->ui->horizontalLayout_5->insertWidget(1, this->followGameButton);
    this->ui->horizontalLayout_5->insertWidget(2, this->applyToGameButton);

    connect(this->followGameButton, &QPushButton::toggled, this, [this](bool checked) {
        //接続中に言語を変更すると、ゲームの翻訳ファイルと列が一致しなくなるため変更を禁止する
        this->ui->langTab->setEnabled(checked == false);
        if(checked == false) { this->mainTable->clearHighlightRow(); }
        this->dispatch(FollowGame, {checked});
        this->updateFollowGameButton();
        //接続中はプラグインの更新・翻訳CSVの書き出しを無効にする。書き出しなどの処理中は無効のまま
        this->changeEnabledUIState(this->fileTree->isEnabledTo(this));
    });
    connect(this->applyToGameButton, &QPushButton::clicked, this, [this]() {
        if(this->setting->isFirstExported == false) {
            //先に翻訳ファイルを書き出して下さい。
            QMessageBox::information(this, tr("Apply to Game"), tr("Please export the translation files first."));
            return;
        }
        this->setHasUnappliedEdits(false);
        this->dispatch(ApplyToGame, {});
    });
    this->updateFollowGameButton();

    connect(this->history, &QUndoStack::indexChanged, this, [this]() { this->setHasUnappliedEdits(true); });
    this->setHasUnappliedEdits(false);
}

void WriteModeComponent::applyEditingTranslationsToExport()
{
    const auto updatedFiles = langscore::ApplyEditingTranslations(*this->setting);
    //編集した翻訳を %1 個のファイルに反映しました。
    this->ui->logText->writeText("\n" + tr("Applied the edited translations to %1 files.").arg(updatedFiles.size()) + "\n");
}

void WriteModeComponent::finishExport(bool openExportFolder)
{
    //テストプレイのための書き出しでは、フォルダーを開かずにゲームを起動する
    if(openExportFolder && testPlayAfterExport == false && applyToGameAfterExport == false) {
        QProcess::startDetached("explorer", {QDir::toNativeSeparators(lastWritePath)});
    }
    applyToGameAfterExport = false;
    this->setHasUnappliedEdits(false);
    this->setting->isFirstExported = true;

    this->dispatch(SaveProject,{});

    if(testPlayAfterExport) {
        testPlayAfterExport = false;
        this->launchTestPlay();
    }
}

//ツクールのテストプレイでゲームを起動する。未保存・未書き出しの編集があれば、書き出すかを確認する
void WriteModeComponent::testPlay()
{
    const auto type = this->setting->projectType;
    //MV・MZ でツクールの場所が不明な場合は、ユーザーに指定させてから続ける
    while(type != settings::VXAce && QFileInfo::exists(RPGMakerPathDialog::testPlayCommand(type, "", RPGMakerPathDialog::rpgMakerPath(type)).first) == false) {
        if(RPGMakerPathDialog(this->window()).exec() != QDialog::Accepted) { return; }
    }

    //初回の書き出し前はゲームにプラグインが無いため、書き出しの確認はしない
    if(this->setting->isFirstExported && (this->history->isClean() == false || langscore::UnexportedEditingFiles(*this->setting).isEmpty() == false))
    {
        QMessageBox box(QMessageBox::Question, tr("Run Game"), tr("There are unsaved or unexported edits.\nDo you want to export them before the test play?"), QMessageBox::NoButton, this);
        auto* exportButton = box.addButton(tr("Export and Play"), QMessageBox::AcceptRole);
        auto* playButton = box.addButton(tr("Play without Exporting"), QMessageBox::DestructiveRole);
        box.addButton(QMessageBox::Cancel);
        box.setDefaultButton(exportButton);
        box.exec();
        if(box.clickedButton() == exportButton)
        {
            this->dispatch(SaveProject, {});
            const auto files = langscore::UnexportedEditingFiles(*this->setting);
            //VXAce は書き出したCSVをパッキング後に削除しているため、divisi で再度書き出してからパッキングする
            if(type == settings::VXAce && files.isEmpty() == false)
            {
                this->changeEnabledUIState(false);
                this->showLogTab();
                this->ui->logText->clear();
                this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));
                testPlayAfterExport = true;
                _invoker->exportCSV();
                invokeType = InvokeType::ExportCSV;
                return;
            }
            //MV・MZ は書き出したCSVが残っているため、内容が変わったファイルにだけ編集を反映する
            if(files.isEmpty() == false) {
                const auto updatedFiles = langscore::ApplyEditingTranslations(*this->setting, files);
                this->ui->logText->writeText("\n" + tr("Applied the edited translations to %1 files.").arg(updatedFiles.size()) + "\n");
            }
        }
        else if(box.clickedButton() != playButton) { return; }
    }
    this->launchTestPlay();
}

void WriteModeComponent::launchTestPlay()
{
    const auto type = this->setting->projectType;
    const auto [program, args] = RPGMakerPathDialog::testPlayCommand(type, this->setting->gameProjectPath, RPGMakerPathDialog::rpgMakerPath(type));
    if(QProcess::startDetached(program, args, this->setting->gameProjectPath) == false) {
        QMessageBox::warning(this, tr("Run Game"), tr("Failed to start the game.\n%1").arg(QDir::toNativeSeparators(program)));
    }
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
    //ゲームへの接続中は、ゲームが読み込んでいるプラグインと翻訳ファイルの書き換えを禁止する
    const bool isFollowingGame = this->followGameButton != nullptr && this->followGameButton->isChecked();
    const bool canExport = enable && isFollowingGame == false;
    this->ui->addPluginButton->setEnabled(enable);
    this->ui->updatePluginButton->setEnabled(canExport);
    this->ui->testPlayButton->setEnabled(enable);
    if(this->applyToGameButton) { this->updateApplyToGameButton(); }
    this->ui->updateButton->setEnabled(enable);
    //言語が1つも選択されていない場合は、処理の完了後も書き出しボタンを無効のままにする
    const bool hasEnabledLanguage = std::ranges::any_of(this->setting->languages, [](const settings::Language& lang) {
        return lang.enable;
    });
    this->ui->writeButton->setEnabled(canExport && hasEnabledLanguage);
}

void WriteModeComponent::updateExportButtonsVisible()
{
    //プラグインの追加前は、追加ボタンのみ表示する
    this->ui->addPluginButton->setVisible(this->setting->isPluginRegistered == false);
    this->ui->exportButtonsWidget->setVisible(this->setting->isPluginRegistered);
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
        //保存前にリモートの変更を取り込み、衝突したセルはどちらの翻訳文にするかユーザーに選択させる
        this->mainTable->syncWithAllFiles();
        this->csvEditDataManager->saveAllModels(editFolder);
        //取り込みから保存までの間に、リモートで同じセルが保存されていた場合
        for(const auto& path : this->csvEditDataManager->mainCSVFiles())
        {
            auto* model = this->csvEditDataManager->findMainCSVModel(path);
            if(model != nullptr) { this->mainTable->resolveMergeConflicts(*model, path); }
        }
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
    else if(type == ComponentBase::NotifyGameRunning)
    {
        if(args.empty()) { return; }
        this->isGameRunning = args[0].toBool();
        this->updateFollowGameButton();
        this->updateApplyToGameButton();
    }
    else if(type == ComponentBase::ShowGameText)
    {
        if(args.size() < 3) { return; }
        const auto original = args[0].toString();
        const auto fileName = args[1].toString();
        //ゲーム: %1
        this->dispatch(StatusMessage, {tr("Game: %1").arg(original.section('\n', 0, 0)), 10000});
        if(fileName.isEmpty() || this->fileTree->selectFile(fileName) == false) {
            this->mainTable->clearHighlightRow();
            return;
        }
        this->mainTable->highlightRowByOriginal(original, args[2].toString());
    }
    else if(type == ComponentBase::ExportForGame)
    {
        this->changeEnabledUIState(false);
        this->showLogTab();
        this->ui->logText->clear();
        this->ui->logText->insertPlainText(tr("Write Translate Files...\n"));
        applyToGameAfterExport = true;
        _invoker->exportCSV();
        invokeType = InvokeType::ExportCSV;
    }
    else if(type == ComponentBase::NotifyApplyToGame)
    {
        if(args.empty()) { return; }
        const auto numFiles = args[0].toInt();
        if(numFiles == 0) {
            //ゲームに未反映の項目はありません。
            this->dispatch(StatusMessage, {tr("There are no edits to apply to the game."), 3000});
            return;
        }
        this->dispatch(StatusMessage, {tr("Applied the edited translations to %1 files.").arg(numFiles), 3000});
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

void WriteModeComponent::updateFollowGameButton()
{
    //ゲームを閉じたら切断する
    if(this->isGameRunning == false && this->followGameButton->isChecked()) {
        this->followGameButton->setChecked(false);
        return;
    }
    this->followGameButton->setEnabled(this->isGameRunning);
    if(this->followGameButton->isChecked()) {
        //接続中...
        this->followGameButton->setText(tr("Connected."));
        //クリックで切断
        this->followGameButton->setToolTip(tr("Click to disconnect"));
        return;
    }
    this->followGameButton->setText(tr("Follow Game"));
    if(this->isGameRunning) {
        //テストプレイ中のゲームで表示している文章の行を示します。
        this->followGameButton->setToolTip(tr("Shows the row of the text displayed in the test playing game."));
    }
    else {
        //テストプレイ中のゲームが見つかりません。「プラグインの更新」でデバッグ用のプラグイン（Langscore_RemoteHub）を追加してから、テストプレイを始めて下さい。
        this->followGameButton->setToolTip(tr("No running game was found.\nAdd Langscore_RemoteHub in \"Update Plugin\", then run the game."));
    }
}

void WriteModeComponent::setHasUnappliedEdits(bool hasEdits)
{
    this->hasUnappliedEdits = hasEdits;
    this->updateApplyToGameButton();
}

//反映はゲームの実行中で、未反映の編集がある場合だけ行える。書き出しなどの処理中は changeEnabledUIState が無効にしている
void WriteModeComponent::updateApplyToGameButton()
{
    this->applyToGameButton->setEnabled(this->isGameRunning && this->hasUnappliedEdits && this->fileTree->isEnabled());
}
