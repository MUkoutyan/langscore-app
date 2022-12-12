#include "MainWindow.h"
#include "ui_MainWindow.h"
#include "./src/ui/FormTaskBar.h"
#include "./src/ui/AnalyzeDialog.h"
#include "./src/ui/WriteModeComponent.h"
#include "./src/ui/PackingMode.h"

#include <QFile>
#include <QDir>
#include <QMouseEvent>
#include <QSettings>
#include <QSizeGrip>
#include <QGraphicsDropShadowEffect>
#include <QPalette>
#include <QFontDatabase>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : FramelessWindow(parent)
    , ComponentBase()
    , ui(new Ui::MainWindow)
    , taskBar(new FormTaskBar(this->history, this))
    , analyzeDialog(new AnalyzeDialog(this, this))
    , writeUi(new WriteModeComponent(this, this))
    , packingUi(new PackingMode(this, this))
    , undoView(nullptr)
    , initialAnalysis(false)
    , explicitSave(false)
    , lastSavedHistoryIndex(0)
    , currentTheme(FormTaskBar::Theme::None)
{
    ui->setupUi(this);
    this->setObjectName("mainWindow");
    this->dispatchComponentList->emplace_back(this);

    this->ui->mainStackedWidget->removeWidget(this->ui->writeMode);
    this->ui->mainStackedWidget->removeWidget(this->ui->packingMode);
    this->ui->verticalLayout->insertWidget(0, taskBar);
    this->ui->verticalLayout->insertWidget(1, writeUi);
    this->ui->verticalLayout->setStretch(1,1);

    analyzeDialog->setContentsMargins(8,0,8,0);
    analyzeDialog->show();

    writeUi->setEnabled(false);
    writeUi->setGraphicsEffect(new QGraphicsBlurEffect);

    this->ui->mainStackedWidget->addWidget(this->writeUi);
    this->ui->mainStackedWidget->addWidget(this->packingUi);

    connect(this->writeUi, &WriteModeComponent::toNextPage, this, [this](){
        this->ui->mainStackedWidget->setCurrentWidget(this->packingUi);
    });

    connect(this->packingUi, &PackingMode::toPrevPage, this, [this](){
        this->ui->mainStackedWidget->setCurrentWidget(this->writeUi);
    });

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
        this->receive(SaveProject, {});
        this->receive(StatusMessage, {tr("Save Projet."), 5000});
    });
    connect(this->taskBar, &FormTaskBar::requestOpenProj, this, [this](QString path)
    {
        const auto configFilePath = path+"_langscore/config.json";
        if(QFile::exists(configFilePath)){
            this->setting->setGameProjectPath(path);
            this->openGameProject(std::move(configFilePath));
        }
        else{
            this->analyzeDialog->openFile(std::move(path));
        }
    });
    connect(this->taskBar, &FormTaskBar::changeTheme, this, &MainWindow::attachTheme);
    connect(this->taskBar, &FormTaskBar::quit,            this, &MainWindow::close);

    connect(this->analyzeDialog, &AnalyzeDialog::toAnalyzeMode, this, [this](){
        initialAnalysis = true;
    });
    connect(this->analyzeDialog, &AnalyzeDialog::toWriteMode, this, [this](QString gameProjPath)
    {
        auto projFile = this->setting->langscoreProjectDirectory+"/config.json";
        if(QFile::exists(projFile) == false){ return; }
        this->openGameProject(std::move(projFile));
    });

    connect(this->history, &QUndoStack::indexChanged, this, [this](int idx){
        if(this->lastSavedHistoryIndex != idx){
            this->setWindowTitle("Langscore " + tr("Edited"));
        }else{
            this->setWindowTitle("Langscore");
        }
    });


    {
        auto theme = (FormTaskBar::Theme)(ComponentBase::getAppSettings().value("appTheme", 2).toInt());
        attachTheme(theme);
    }
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openGameProject(QString configFilePath)
{
    this->writeUi->clear();

    this->setting->load(std::move(configFilePath));
    this->history->clear();

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
        if(this->initialAnalysis && this->explicitSave == false){
            QDir(this->setting->langscoreProjectDirectory).removeRecursively();
        }
        e->accept();
    }
    else if(button == QMessageBox::Save){
        this->setting->saveForProject();
        e->accept();
    }
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
        lastSavedHistoryIndex = this->history->index();
    }
}

int MainWindow::askCloseProject()
{
    bool ask = this->initialAnalysis && (this->explicitSave == false);

    if(ask == false && (this->history->index() == this->lastSavedHistoryIndex)){
        return QMessageBox::Discard;
    }
    //ファイルを閉じようとしています。
    //プロジェクト加えた変更を保存しますか？
    auto button = QMessageBox::question(this, tr("Trying to close a project."), tr("Do you want to save the changes you made to the project?\nIf not, the changes will be discarded."), QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel);
    return (int)button;
}

void MainWindow::attachTheme(FormTaskBar::Theme theme)
{
    if(theme == FormTaskBar::Theme::System)
    {
#ifdef Q_OS_WIN
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
        if(settings.value("AppsUseLightTheme")==1){
            theme = FormTaskBar::Theme::Light;
        }
        else{
            theme = FormTaskBar::Theme::Dark;
        }
#endif
    }

    if(currentTheme == theme){ return; }
    currentTheme = theme;

    if(theme == FormTaskBar::Theme::Dark){
        QPalette darkPalette;
        QColor darkColor = QColor(28, 28, 28);
        QColor disabledColor = QColor(127,127,127);
        darkPalette.setColor(QPalette::Window, darkColor);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
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
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(darkPalette);

        qApp->setStyleSheet("QToolTip { color: #efefef; background-color: #222222; border: 1px solid white; }");

        this->setStyleSheet("#mainWindow{ border: 2px solid #323232; }");
        analyzeDialog->setStyleSheet("#analyzeDialog{border: 2px solid #3f3f3f;}");
    }
    else if(theme == FormTaskBar::Theme::Light){
        QPalette darkPalette;
        QColor whiteColor = QColor(240, 240, 240);
        QColor disabledColor = QColor(127,127,127);
        darkPalette.setColor(QPalette::Window, whiteColor);
        darkPalette.setColor(QPalette::WindowText, Qt::black);
        darkPalette.setColor(QPalette::Base, QColor(230, 230, 230));
        darkPalette.setColor(QPalette::AlternateBase, whiteColor);
        darkPalette.setColor(QPalette::ToolTipBase, Qt::black);
        darkPalette.setColor(QPalette::ToolTipText, Qt::black);
        darkPalette.setColor(QPalette::Text, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, whiteColor);
        darkPalette.setColor(QPalette::ButtonText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, QColor(139, 25, 40));
        darkPalette.setColor(QPalette::Link, QColor(99, 166, 233));

        darkPalette.setColor(QPalette::Highlight, QColor(99, 166, 233));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(darkPalette);

        qApp->setStyleSheet("QToolTip { color: #222222; background-color: #efefef; border: 1px solid black; }");

        this->setStyleSheet("#mainWindow{ border: 2px solid #aaaaaa; }");
        analyzeDialog->setStyleSheet("#analyzeDialog{border: 2px solid #999999;}");
    }
}

void MainWindow::createUndoView()
{
    undoView = new QUndoView(this->history);
    undoView->setWindowTitle(tr("Command List"));
    undoView->show();
    undoView->setAttribute(Qt::WA_QuitOnClose, false);
}
