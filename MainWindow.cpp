#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "./src/ui/FormTaskBar.h"
#include "./src/ui/dialog/AnalyzeDialog.h"
#include "./src/ui/WriteModeComponent.h"
#include "./src/invoker.h"

#include <QFile>
#include <QDir>
#include <QMouseEvent>
#include <QSettings>
#include <QSizeGrip>
#include <QGraphicsDropShadowEffect>
#include <QPalette>
#include <QFontDatabase>
#include <QMessageBox>
#include <QFileDialog>
#include <QTimer>   

using namespace langscore;

MainWindow::MainWindow(QWidget *parent)
    : FramelessWindow(parent)
    , ComponentBase()
    , ui(new Ui::MainWindow)
    , taskBar(new FormTaskBar(this->history, this))
    , analyzeDialog(new AnalyzeDialog(this, this))
    , writeUi(new WriteModeComponent(this, this))
    , validationService(new ValidationService(this))
    , followGameService(new FollowGameService(this))
    , undoView(nullptr)
    , initialAnalysis(false)
    , explicitSave(false)
{
    ui->setupUi(this);
    this->setObjectName("mainWindow");
    this->dispatchComponentList->emplace_back(this);

    this->ui->mainStackedWidget->removeWidget(this->ui->writeMode);
    this->ui->verticalLayout->insertWidget(0, taskBar);
    this->ui->verticalLayout->insertWidget(1, writeUi);
    this->ui->verticalLayout->setStretch(1,1);

    analyzeDialog->setContentsMargins(8,0,8,0);
    analyzeDialog->show();

    writeUi->setEnabled(false);
    writeUi->setGraphicsEffect(new QGraphicsBlurEffect);

    this->ui->mainStackedWidget->addWidget(this->writeUi);

    this->addDispatch(validationService);


    connect(this->taskBar, &FormTaskBar::showUndoView, this, &MainWindow::createUndoView);

    connect(this->taskBar, &FormTaskBar::pushClose,   this, &QMainWindow::close);
    connect(this->taskBar, &FormTaskBar::maximum,     this, &FramelessWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::doubleClick, this, &FramelessWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::minimum,     this, &QMainWindow::showMinimized);

    connect(this->taskBar, &FormTaskBar::dragging, this, [this](QMouseEvent*, QPoint delta)
    {
        if(mousePressEdge == Edges::None){
            auto pos = this->pos() + delta;
            emit this->moveWiget(delta);
            this->move(pos);
        }
    });

    connect(this, &MainWindow::moveWiget, this->analyzeDialog, &AnalyzeDialog::moveParent);
    connect(this->taskBar, &FormTaskBar::saveProj,        this, [this](){
        this->explicitSave = true;
        this->dispatch(SaveProject, {});
        this->writeUi->updateExportDirectory();
        this->receive(StatusMessage, {tr("Project saved."), 5000});
    });
    connect(this->taskBar, &FormTaskBar::requestOpenProj, this, [this](QString path)
    {
        //別のプロジェクトを読み込む前に、現在のプロジェクトの未保存の変更を確認する。
        //setGameProjectPathの後だと保存先が新しいプロジェクトになるため、ここで行う
        const auto button = this->askCloseProject();
        if(button == QMessageBox::Cancel) { return; }
        if(button == QMessageBox::Save) { this->dispatch(SaveProject, {}); }

        const auto configFilePath = path+"_langscore/config.json";
        if(QFile::exists(configFilePath)){
            this->initialAnalysis = false;
            this->setting->setGameProjectPath(path);
            this->openGameProject(std::move(configFilePath));
        }
        else{
            this->analyzeDialog->openFile(std::move(path));
        }
    });
    connect(this->taskBar, &FormTaskBar::validationAll, this, [this]() {
        //divisiでアプリ側エディターのCSVを検証するため、未表示のファイルも読み込んでから保存する
        this->writeUi->loadAllMainFiles();
        this->dispatch(SaveProject, {});
        this->dispatch(ValidateAll, {});
    });
    connect(this->taskBar, &FormTaskBar::importTranslationCsv, this, [this]()
    {
        //プロジェクトを開いていない間は取り込み先が無い
        if(this->writeUi->isEnabled() == false) { return; }
        //外部エディターでは書き出した CSV を直接編集していたため、書き出し先から選択する
        const QDir langscoreProjectDir(this->setting->langscoreProjectDirectory);
        const auto exportFolder = langscoreProjectDir.absoluteFilePath(this->setting->writeObj.exportDirectory);
        const auto csvPaths = QFileDialog::getOpenFileNames(this, tr("Import Translation CSV"), exportFolder, tr("CSV (*.csv)"));
        if(csvPaths.isEmpty()) { return; }
        const auto message = this->writeUi->importTranslationCsvFiles(csvPaths);
        QMessageBox::information(this, tr("Import Translation CSV"), message);
    });
    //connect(this->taskBar, &FormTaskBar::changeTheme, this, &MainWindow::attachTheme);
    connect(this->taskBar, &FormTaskBar::quit,            this, &MainWindow::close);

    connect(this->analyzeDialog, &AnalyzeDialog::toAnalyzeMode, this, [this](){
        initialAnalysis = true;
        initialAnalysisProjectDir = this->setting->langscoreProjectDirectory;
    });
    connect(this->analyzeDialog, &AnalyzeDialog::toWriteMode, this, [this](QString gameProjPath)
    {
        auto projFile = this->setting->langscoreProjectDirectory+"/config.json";
        if(QFile::exists(projFile) == false){ return; }
        //解析済みの別プロジェクトを開いた場合は、破棄時の削除など初回解析としての扱いをしない
        if(this->setting->langscoreProjectDirectory != initialAnalysisProjectDir) {
            this->initialAnalysis = false;
        }
        this->openGameProject(std::move(projFile));
    });
    connect(this->analyzeDialog, &AnalyzeDialog::translationStarted, this, [this]()
    {
        //ゲームにプラグインと翻訳CSVを書き出したため、破棄しても Langscore のプロジェクトは消さない
        this->initialAnalysis = false;
    });

    //再解析やプロジェクトの切り替えで履歴をクリアしたときも保存済みの状態になるよう、履歴の clean 状態で判定する
    connect(this->history, &QUndoStack::cleanChanged, this, [this](bool clean){
        this->setWindowTitle(clean ? "Langscore" : "Langscore " + tr("Edited"));
    });

    {
        attachTheme(ComponentBase::getColorTheme().getCurrentTheme());

        auto&& settings = this->getAppSettings();
        // geometryにはウィンドウの位置とサイズが保存されています
        if(settings.contains("geometry") == true) {
            this->restoreGeometry(settings.value("geometry").toByteArray());
        }

        // windowStateにはウィンドウの最大化などの状態が保存されています
        if(settings.contains("windowState") == true) {
            this->restoreState(settings.value("windowState").toByteArray());
        }

        this->update();

        QTimer::singleShot(0, this, [this]() {
            auto parentRect = this->rect();
            auto analyzeRect = this->analyzeDialog->rect();
            qDebug() << "Parent Rect:" << parentRect;
            analyzeRect.moveCenter(parentRect.center());
            qDebug() << "Analyze Dialog Rect (parent coords):" << analyzeRect;
            // AnalyzeDialog はトップレベルのウィンドウのため、親のローカル座標をグローバルへ変換して移動する
            this->analyzeDialog->move(this->mapToGlobal(analyzeRect.topLeft()));
        });
    }

}

MainWindow::~MainWindow()
{
    //ゲームの確認のタイマーが、破棄した画面へ dispatch しないよう先に破棄する
    delete followGameService;
    delete ui;
}

void MainWindow::openGameProject(QString configFilePath)
{
    this->explicitSave = false;
    this->writeUi->clear();

    this->setting->load(std::move(configFilePath));
    //新しいプロジェクトや、editing が無い以前のバージョンのプロジェクトでも、最初に作成する。
    //初回保存前から editing を監視し、共有先でのリモートの保存を検知できるようにする
    QDir().mkpath(this->setting->langscoreProjectDirectory + "/editing");
    this->history->clear();
    this->askMoveExportDirectory();

    auto&& settings = this->getAppSettings();
    auto projDir = this->setting->gameProjectPath;
    QVariantList recentFiles;
    auto recentList = settings.value("recentFiles", recentFiles).toList();
    if(recentList.size() >= 8){
        recentList.remove(7, qMin(1, recentList.size()-7));
    }

    auto rm_result = std::remove_if(recentList.begin(), recentList.end(), [&](auto obj){
        return settings::getProjectType(obj.toString()) == settings::ProjectType::None;
    });
    recentList.erase(rm_result, recentList.end());

    int index = 0;
    for(auto& obj : recentList)
    {
        auto path = obj.toString();
        if(path == projDir){
            recentList.removeAt(index);
            break;
        }
        ++index;
    }
    recentList.insert(0, QVariant(projDir));
    settings.setValue("recentFiles", recentList);
    settings.sync();

    this->showWriteMode();
}

//以前のバージョンでは langscore のフォルダの中に書き出していた。ゲームが読み込む翻訳フォルダへ変更するかを確認する。
//「いいえ」を選んだプロジェクトでは次から確認しない
void MainWindow::askMoveExportDirectory()
{
    if(this->setting->isExportDirectoryInLangscoreFolder() == false) { return; }
    auto&& appSettings = this->getAppSettings();
    const auto key = "KeepExportDirectory/" + QString(this->setting->langscoreProjectDirectory.toUtf8().toBase64());
    if(appSettings.value(key, false).toBool()) { return; }

    const auto newFolder = QDir::toNativeSeparators(this->setting->translateDirectoryPath());
    const auto button = QMessageBox::question(this, tr("Change Export Folder"),
        tr("The export folder of this project is inside the Langscore project folder.\n"
           "From this version, translations are exported to the game's translation folder.\n"
           "Change the export folder to the following folder?\n\n%1").arg(newFolder),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if(button == QMessageBox::Yes) {
        this->setting->useGameTranslateFolderForExport();
        this->setting->saveForProject();
    }
    else {
        appSettings.setValue(key, true);
    }
}

void MainWindow::showWriteMode()
{
    this->taskBar->updateRecentMenu();

    this->writeUi->show();
    this->writeUi->setEnabled(true);
    this->writeUi->setGraphicsEffect({});
    this->analyzeDialog->hide();
    this->update();
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    auto button = this->askCloseProject();
    if(button == QMessageBox::Cancel){
        e->ignore();
        return;
    }
    else if(button == QMessageBox::Discard)
    {
        //初回解析したプロジェクト以外のフォルダは決して削除しない
        if(this->initialAnalysis && this->explicitSave == false &&
           this->setting->langscoreProjectDirectory == initialAnalysisProjectDir){
            QDir(this->setting->langscoreProjectDirectory).removeRecursively();
        }
        e->accept();
    }
    else if(button == QMessageBox::Save){
        this->dispatch(SaveProject, {});
        e->accept();
    }

    auto&& settings = this->getAppSettings();
    settings.setValue("geometry", this->saveGeometry());
    settings.setValue("windowState", this->saveState());

    QMainWindow::closeEvent(e);
}

void MainWindow::receive(DispatchType type, const QVariantList &args)
{
    if(type == DispatchType::StatusMessage){
        if(args.empty()){ return; }
        auto mes = args[0].toString();
        auto time = args.size()==2 ? args[1].toInt() : 0;
        this->ui->statusBar->showMessage(mes, time);
    }
    else if(type == DispatchType::SaveProject)
    {
        this->setting->saveForProject();
        this->history->setClean();
    }
    else if(type == DispatchType::ChangeColor)
    {
        if(args.empty()){ return; }

        bool isOk = false;
        auto value = args[0].toInt(&isOk);
        if(isOk == false){ return; }

        auto theme = static_cast<ColorTheme::Theme>(value);
        this->attachTheme(theme);
    }
}

int MainWindow::askCloseProject()
{
    bool ask = this->initialAnalysis && (this->explicitSave == false);

    if(ask == false && this->history->isClean()){
        return QMessageBox::Discard;
    }
    //ファイルを閉じようとしています。
    //プロジェクト加えた変更を保存しますか？
    auto button = QMessageBox::question(this, tr("Closing the project."), tr("Do you want to save the changes you made to the project?\nIf you choose No, your edits will be discarded."), QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel);
    return (int)button;
}

void MainWindow::attachTheme(ColorTheme::Theme theme)
{
    if(theme == ColorTheme::Theme::Dark){
        QPalette darkPalette;
        QColor darkColor = QColor(28, 28, 28);
        QColor disabledColor = QColor(127,127,127);
        darkPalette.setColor(QPalette::Window, darkColor);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::WindowText, disabledColor);
        darkPalette.setColor(QPalette::Base, QColor(52, 52, 52));
        darkPalette.setColor(QPalette::AlternateBase, darkColor);
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, darkColor);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, QColor(139, 25, 40));
        darkPalette.setColor(QPalette::Link, QColor(74, 124, 149));

        darkPalette.setColor(QPalette::Highlight, QColor(74, 124, 149));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(darkPalette);

        qApp->setStyleSheet(R"(
QToolTip { color: #efefef; background-color: #222222; border: 1px solid white; }
QTableView::item:hover { background-color: #2202abd1;}
QTreeView::item:hover { background-color: #2202abd1;}
)");

        this->setStyleSheet("#mainWindow{ border: 2px solid #323232; }");
        analyzeDialog->setStyleSheet("#analyzeDialog{border: 2px solid #3f3f3f;}");
    }
    else if(theme == ColorTheme::Theme::Light){
        QPalette lightPalette;
        QColor whiteColor = QColor(240, 240, 240);
        QColor disabledColor = QColor(127,127,127);
        lightPalette.setColor(QPalette::Window, whiteColor);
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::Base, QColor(230, 230, 230));
        lightPalette.setColor(QPalette::AlternateBase, QColor(220, 220, 220));
        lightPalette.setColor(QPalette::ToolTipBase, Qt::black);
        lightPalette.setColor(QPalette::ToolTipText, Qt::black);
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        lightPalette.setColor(QPalette::Button, whiteColor);
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        lightPalette.setColor(QPalette::BrightText, QColor(139, 25, 40));
        lightPalette.setColor(QPalette::Link, QColor(99, 166, 233));

        lightPalette.setColor(QPalette::Highlight, QColor(99, 166, 233));
        lightPalette.setColor(QPalette::HighlightedText, Qt::black);
        lightPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(lightPalette);

        qApp->setStyleSheet(R"(
QToolTip { color: #222222; background-color: #efefef; border: 1px solid gray; }
QTableView::item:hover { background-color: #2202abd1;}
QTreeView::item:hover { background-color: #2202abd1;}
)");

        this->setStyleSheet("#mainWindow{ border: 2px solid #aaaaaa; }");
        analyzeDialog->setStyleSheet("#analyzeDialog{border: 2px solid #999999;}");
    }
}


void MainWindow::createUndoView()
{
    undoView = new QUndoView(this->history);
    undoView->setWindowTitle(tr("Edit History"));
    undoView->show();
    undoView->setAttribute(Qt::WA_QuitOnClose, false);
}
