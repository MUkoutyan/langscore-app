#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "./src/ui/FormTaskBar.h"
#include "./src/ui/MainComponent.h"

#include <QStandardPaths>
#include <QMouseEvent>
#include <QSettings>
#include <QSizeGrip>
#include <QGraphicsDropShadowEffect>
#include <QFileDialog>
#include <QPalette>


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ComponentBase()
    , ui(new Ui::MainWindow)
    , taskBar(new FormTaskBar(this->history, this))
    , mainComponent(new MainComponent(this, this))
    , undoView(nullptr)
    , mousePressEdge(Edges::None)
    , mouseMoveEdge(Edges::None)
    , mousePressPos()
    , leftButtonPressed(false)
    , isDragging(false)
    , cursorChanged(false)
    , draggingStartPos()
    , borderWidth(2)
{
    ui->setupUi(this);
    this->setObjectName("mainWindow");
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_Hover);
    this->setAutoFillBackground(true);
    mainComponent->setContentsMargins(8,0,8,0);

    this->ui->verticalLayout->insertWidget(0, taskBar);
    this->ui->verticalLayout->insertWidget(1, mainComponent);
    this->ui->verticalLayout->setStretch(1,1);

    connect(this->taskBar, &FormTaskBar::showUndoView, this, &MainWindow::createUndoView);

    connect(this->taskBar, &FormTaskBar::pushClose,   this, &QMainWindow::close);
    connect(this->taskBar, &FormTaskBar::maximum,     this, &MainWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::doubleClick, this, &MainWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::minimum,     this, &QMainWindow::showMinimized);

    connect(this->taskBar, &FormTaskBar::dragging, this, [this](QMouseEvent* event, QPoint delta)
    {
        if(mousePressEdge == Edges::None){
            auto pos = this->pos() + delta;
            this->move(pos);
        }
    });

    connect(this->mainComponent, &MainComponent::requestOpenOutputDir, this, &MainWindow::openOutputProjectDir);
    connect(this->mainComponent, &MainComponent::openProject, this->taskBar, &FormTaskBar::updateRecentMenu);

    connect(this->taskBar, &FormTaskBar::openGameProj, this, [this]()
    {
        auto&& settings = ComponentBase::getAppSettings();
        auto openPath = settings.value("lastOpenGameProjDirectory", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)).toUrl();
        openPath = QFileDialog::getExistingDirectory(this, tr("Open Game Project Folder..."), openPath.toString());
        if(openPath.isEmpty()){ return; }
        settings.setValue("lastOpenGameProjDirectory", openPath);
        settings.sync();
        this->openGameProject(openPath.toString());
    });
    connect(this->taskBar, &FormTaskBar::saveProj,        this, [this](){
        this->setting->saveForProject();
        this->ui->statusBar->showMessage(tr("Save Projet."), 5000);
    });
    connect(this->taskBar, &FormTaskBar::requestOpenProj, this, &MainWindow::openGameProject);
    connect(this->taskBar, &FormTaskBar::quit,            this, [](){
        qApp->exit(0);
    });

#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
    if(settings.value("AppsUseLightTheme")==0){
        QPalette darkPalette;
        QColor darkColor = QColor(45,45,45);
        QColor disabledColor = QColor(127,127,127);
        darkPalette.setColor(QPalette::Window, darkColor);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(18,18,18));
        darkPalette.setColor(QPalette::AlternateBase, darkColor);
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, darkColor);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(darkPalette);

        qApp->setStyleSheet("QToolTip { color: #efefef; background-color: #2a82da; border: 1px solid white; }");

        this->setStyleSheet("#mainWindow{ border: 2px solid #0f0f0f; }");
    }
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::openGameProject(QString path) {
    this->mainComponent->openGameProject(path);
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

QString MainWindow::openOutputProjectDir(QString root)
{
    return QFileDialog::getExistingDirectory(this, tr("Open Project File"), root);
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
    updateCursorShape(e->globalPosition().toPoint());
}

void MainWindow::mouseLeave(QEvent *e) {
    if (!leftButtonPressed) {
        this->unsetCursor();
    }
}

void MainWindow::mousePress(QMouseEvent *e) {
    if (e->button() & Qt::LeftButton) {
        leftButtonPressed = true;
        calculateCursorPosition(e->globalPosition().toPoint(), mousePressEdge);

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
        updateCursorShape(e->globalPosition().toPoint());
        return;
    }

    if(mousePressEdge == Edges::None){ return; }

    auto rect = this->frameGeometry();
    auto pos = e->globalPosition();
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

void MainWindow::createUndoView()
{
    undoView = new QUndoView(this->history);
    undoView->setWindowTitle(tr("Command List"));
    undoView->show();
    undoView->setAttribute(Qt::WA_QuitOnClose, false);
}
