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
    : QMainWindow(parent)
    , ComponentBase()
    , ui(new Ui::MainWindow)
    , taskBar(new FormTaskBar(this->history, this))
    , analyzeDialog(new AnalyzeDialog(this, this))
    , writeUi(new WriteModeComponent(this, this))
    , packingUi(new PackingMode(this, this))
    , undoView(nullptr)
    , mousePressEdge(Edges::None)
    , mouseMoveEdge(Edges::None)
    , mousePressPos()
    , leftButtonPressed(false)
    , isDragging(false)
    , cursorChanged(false)
    , initialAnalysis(false)
    , explicitSave(false)
    , draggingStartPos()
    , borderWidth(2)
    , lastSavedHistoryIndex(0)
{
    ui->setupUi(this);
    this->setObjectName("mainWindow");
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_Hover);
    this->setAutoFillBackground(true);
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
    connect(this->taskBar, &FormTaskBar::maximum,     this, &MainWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::doubleClick, this, &MainWindow::changeMaximumState);
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
    connect(this->analyzeDialog, &AnalyzeDialog::toAnalyzeMode, this, [this](){
        initialAnalysis = true;
    });
    connect(this->analyzeDialog, &AnalyzeDialog::toWriteMode, this, [this](QString gameProjPath)
    {
        auto projFile = this->setting->langscoreProjectDirectory+"/config.json";
        if(QFile::exists(projFile) == false){ return; }
        this->openGameProject(std::move(projFile));
    });
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
    connect(this->taskBar, &FormTaskBar::quit,            this, &MainWindow::close);

    connect(this->history, &QUndoStack::indexChanged, this, [this](int idx){
        if(this->lastSavedHistoryIndex != idx){
            this->setWindowTitle("Langscore " + tr("Edited"));
        }else{
            this->setWindowTitle("Langscore");
        }
    });

    connect(this->taskBar, &FormTaskBar::changeTheme, this, &MainWindow::attachTheme);

    attachTheme(FormTaskBar::Theme::System);

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

bool MainWindow::changeMaximumState()
{
    if(this->isMaximized()){
        this->showNormal();
        return false;
    }
    else {
        this->showMaximized();
        return true;
    }
}

bool MainWindow::event(QEvent*e)
{
    switch (e->type()) {
    case QEvent::MouseMove:
        mouseMove(static_cast<QMouseEvent*>(e));
        break;
    case QEvent::HoverMove:
        mouseHover(static_cast<QHoverEvent*>(e));
        break;
    case QEvent::Leave:
        mouseLeave(e);
        break;
    case QEvent::MouseButtonPress:
        mousePress(static_cast<QMouseEvent*>(e));
        break;
    case QEvent::MouseButtonRelease:
        mouseRealese(static_cast<QMouseEvent*>(e));
        break;
    default:
        break;
    }
    return QMainWindow::event(e);
}


void MainWindow::mouseHover(QHoverEvent *e) {
    updateCursorShape(QCursor::pos());
}

void MainWindow::mouseLeave(QEvent*) {
    if (!leftButtonPressed) {
        this->unsetCursor();
    }
}

void MainWindow::mousePress(QMouseEvent *e) {
    if (e->button() & Qt::LeftButton) {
        leftButtonPressed = true;
        calculateCursorPosition(QCursor::pos(), mousePressEdge);

        if (this->rect().marginsRemoved(QMargins(borderWidth, borderWidth, borderWidth, borderWidth)).contains(e->pos())) {
            isDragging = true;
            draggingStartPos = e->pos();
        }
    }
}

void MainWindow::mouseRealese(QMouseEvent *e) {
    if (e->button() & Qt::LeftButton) {
        leftButtonPressed = false;
        isDragging = false;
        mousePressPos = QPoint();
    }
}

void MainWindow::mouseMove(QMouseEvent *e)
{
    if (leftButtonPressed == false) {
        updateCursorShape(QCursor::pos());
        return;
    }

    if(mousePressEdge == Edges::None){ return; }

    auto rect = this->frameGeometry();
    auto pos = QCursor::pos();
    int top = rect.top(), bottom = rect.bottom(), left = rect.left(), right = rect.right();

    switch (mousePressEdge) {
    case Edges::Top:
        top = pos.y();
        break;
    case Edges::Bottom:
        bottom = pos.y();
        break;
    case Edges::Left:
        left = pos.x();
        break;
    case Edges::Right:
        right = pos.x();
        break;
    case Edges::TopLeft:
        top = pos.y();
        left = pos.x();
        break;
    case Edges::TopRight:
        right = pos.x();
        top = pos.y();
        break;
    case Edges::BottomLeft:
        bottom = pos.y();
        left = pos.x();
        break;
    case Edges::BottomRight:
        bottom = pos.y();
        right = pos.x();
        break;
    default:
        break;
    }
    QRect newRect(QPoint(left, top), QPoint(right, bottom));
    if (newRect.width() < this->minimumWidth()) {
        newRect.setLeft(this->frameGeometry().x());
    }
    else if (newRect.height() < this->minimumHeight()) {
        newRect.setTop(this->frameGeometry().y());
    }
    this->resize(newRect.size());
    this->move(newRect.topLeft());
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

void MainWindow::updateCursorShape(const QPoint &pos)
{
    if (this->isFullScreen() || this->isMaximized()) {
        if (cursorChanged) {
            this->unsetCursor();
        }
        return;
    }
    if(leftButtonPressed){ return; }

    calculateCursorPosition(pos, mouseMoveEdge);
    cursorChanged = true;
    if (mouseMoveEdge&Edges::Top || mouseMoveEdge&Edges::Bottom) {
        this->setCursor(Qt::SizeVerCursor);
    }
    else if (mouseMoveEdge&Edges::Left || mouseMoveEdge&Edges::Right) {
        this->setCursor(Qt::SizeHorCursor);
    }
    else if (mouseMoveEdge&Edges::TopLeft || mouseMoveEdge&Edges::BottomRight) {
        this->setCursor(Qt::SizeFDiagCursor);
    }
    else if (mouseMoveEdge&Edges::TopRight || mouseMoveEdge&Edges::BottomLeft) {
        this->setCursor(Qt::SizeBDiagCursor);
    }
    else if (cursorChanged) {
        this->unsetCursor();
        cursorChanged = false;
    }
}

void MainWindow::calculateCursorPosition(const QPoint &pos, Edges &_edge)
{
    auto rect = this->frameGeometry();
    auto border = borderWidth*2;

    if (QRect(rect.x()-border, rect.bottom()-border, border*2, border*2).contains(pos)) {
        _edge = BottomLeft;
    }
    else if (QRect(rect.right()-border, rect.bottom()-border, border*2, border*2).contains(pos)) {
        _edge = BottomRight;
    }
    else if (QRect(rect.right()-border, rect.top(), border*2, border).contains(pos)) {
        _edge = TopRight;
    }
    else if (QRect(rect.left()-border, rect.top(), border*2, border).contains(pos)) {
        _edge = TopLeft;
    }
    else if (QRect(rect.x()-border, rect.y()+border, border*2, rect.height()-border).contains(pos)) {
        _edge = Left;
    }
    else if (QRect(rect.right()-border, rect.y()+border, border*2, rect.height()-border).contains(pos)) {
        _edge = Right;
    }
    else if (QRect(rect.x()+border, rect.bottom()-border, rect.width(), border*2).contains(pos)) {
        _edge = Bottom;
    }
    else if (QRect(rect.x()+border, rect.top()-border, rect.width(), border*2).contains(pos)) {
        _edge = Top;
    }
    else {
        _edge = None;
    }
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
    bool darkTheme = false;
    bool lightTheme = false;
    switch(theme)
    {
    case FormTaskBar::Theme::Dark:
        darkTheme = true;
        break;
    case FormTaskBar::Theme::Light:
        lightTheme = true;
        break;
    case FormTaskBar::Theme::System:
#ifdef Q_OS_WIN
        QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
        if(settings.value("AppsUseLightTheme")==1){
            lightTheme = true;
        }
        else{
            darkTheme = true;
        }
#endif
        break;
    }

    if(darkTheme){
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

        qApp->setStyleSheet("QToolTip { color: #efefef; background-color: #2a82da; border: 1px solid white; }");

        this->setStyleSheet("#mainWindow{ border: 2px solid #323232; }");
    }
}

void MainWindow::createUndoView()
{
    undoView = new QUndoView(this->history);
    undoView->setWindowTitle(tr("Command List"));
    undoView->show();
    undoView->setAttribute(Qt::WA_QuitOnClose, false);
}
