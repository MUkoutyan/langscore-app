#include "FormTaskBar.h"
#include "ui_FormTaskBar.h"
#include "ComponentBase.h"
#include "../graphics.hpp"

#include "ColorDialog.h"

#include <QActionGroup>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStandardPaths>
#include <QFileDialog>
#include <QFile>
#include <QDir>


FormTaskBar::FormTaskBar(QUndoStack *history, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FormTaskBar)
    , pressLeftButton(false)
    , history(history)
    , recentProjMenu(nullptr)
{
    ui->setupUi(this);



    this->ui->closeButton->setStyleSheet(R"(
        #closeButton{
            border: none;
            background-color: transparent;
        }
        #closeButton:hover{background-color: rgba(255,32,32, 200);}
        #closeButton:hover:pressed{background-color: rgba(255,32,32, 98);}
    )");
    this->ui->maximumButton->setStyleSheet(R"(
        #maximumButton{
            border: none;
            background-color: transparent;
        }
        #maximumButton:hover{background-color: rgba(255,255,255,92);}
        #maximumButton:hover:pressed{background-color: rgba(255,255,255,32);}
    )");
    this->ui->minimumButton->setStyleSheet(R"(
        #minimumButton{
            border: none;
            background-color: transparent;
        }
        #minimumButton:hover{background-color: rgba(255,255,255,92);}
        #minimumButton:hover:pressed{background-color: rgba(255,255,255,32);}
    )");

    SetIconWithReverceColor(this->ui->closeButton, ":images/resources/image/close.svg");
    SetIconWithReverceColor(this->ui->maximumButton, ":images/resources/image/maxminze.svg");
    SetIconWithReverceColor(this->ui->minimumButton, ":images/resources/image/minimize.svg");

    connect(this->ui->closeButton,   &QPushButton::clicked, this, &FormTaskBar::pushClose);
    connect(this->ui->maximumButton, &QPushButton::clicked, this, [this](){
        if(emit this->maximum()){
            SetIconWithReverceColor(this->ui->maximumButton, ":/images/resources/image/normalize.svg");
        }
        else{
            SetIconWithReverceColor(this->ui->maximumButton, ":/images/resources/image/maxminze.svg");
        }
    });
    connect(this->ui->minimumButton, &QPushButton::clicked, this, &FormTaskBar::minimum);

    auto menuBar = new QMenuBar(this);
    menuBar->setStyleSheet("QMenuBar{ border: none; spacing: 8px; }");
    auto menuBarFont = menuBar->font();
    menuBarFont.setPixelSize(16);
    menuBar->setFont(menuBarFont);
    this->ui->horizontalLayout_2->insertWidget(0, menuBar);

    const auto AddMenu = [menuBar](QString text){
        auto menu = menuBar->addMenu(text);
        menu->setContentsMargins(8,2,8,2);
        return menu;
    };

    //File Menu
    auto fileMenu = AddMenu(tr("File"));
    auto openGameProj = fileMenu->addAction(tr("Open Game Project..."));
    auto saveProj     = fileMenu->addAction(tr("Save Langscore Project..."));
    recentProjMenu = fileMenu->addMenu(tr("Recent Project"));
    updateRecentMenu();

    auto quit = fileMenu->addAction(tr("Quit"));

    connect(openGameProj, &QAction::triggered, this, [this]()
    {
        auto&& settings = ComponentBase::getAppSettings();
        auto openPath = settings.value("lastOpenGameProjDirectory", QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)).toUrl();
        openPath = QFileDialog::getExistingDirectory(this, tr("Open Game Project Folder..."), openPath.toString());
        if(openPath.isEmpty()){ return; }
        settings.setValue("lastOpenGameProjDirectory", openPath);
        settings.sync();
        emit this->requestOpenProj(openPath.toLocalFile());
    });
    connect(saveProj,     &QAction::triggered, this, &FormTaskBar::saveProj);
    connect(quit,         &QAction::triggered, this, &FormTaskBar::quit);

    //Edit Menu
    auto editMenu = AddMenu(tr("Edit"));
    auto undoAction = history->createUndoAction(this, tr("Undo"));
    undoAction->setShortcut(Qt::CTRL|Qt::Key_Z);
    editMenu->addAction(undoAction);
    auto redoAction = history->createRedoAction(this, tr("Redo"));
    redoAction->setShortcut(Qt::CTRL|Qt::SHIFT|Qt::Key_Z);
    editMenu->addAction(redoAction);
    editMenu->addSeparator();
    auto undoView = editMenu->addAction(tr("Show Undo View..."));

    connect(undoAction, &QAction::triggered, this, &FormTaskBar::undo);
    connect(redoAction, &QAction::triggered, this, &FormTaskBar::redo);
    connect(undoView,   &QAction::triggered, this, &FormTaskBar::showUndoView);

    //System Menu
    auto systemMenu = AddMenu(tr("System"));
    auto themeAction        = systemMenu->addMenu(tr("Theme"));
    auto lightAction        = themeAction->addAction(tr("Light"));
    lightAction->setCheckable(true);
    auto darkAction         = themeAction->addAction(tr("Dark"));
    darkAction->setCheckable(true);
    auto systemThemeAction  = themeAction->addAction(tr("System"));
    systemThemeAction->setCheckable(true);
    QActionGroup* themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    themeGroup->addAction(lightAction);
    themeGroup->addAction(darkAction);
    themeGroup->addAction(systemThemeAction);
    connect(lightAction,       &QAction::triggered, this, std::bind(&FormTaskBar::emitChangeTheme, this, Theme::Light));
    connect(darkAction,        &QAction::triggered, this, std::bind(&FormTaskBar::emitChangeTheme, this, Theme::Dark));
    connect(systemThemeAction, &QAction::triggered, this, std::bind(&FormTaskBar::emitChangeTheme, this, Theme::System));

    {
        auto&& settings = ComponentBase::getAppSettings();
        auto theme = (Theme)settings.value("appTheme", 2).toInt();
        switch(theme){
            case Theme::Dark:   darkAction->setChecked(true); break;
            case Theme::Light:  lightAction->setChecked(true); break;
            case Theme::System: systemThemeAction->setChecked(true); break;
        }
    }

    auto colorPalette = systemMenu->addAction(tr("Color"));
    connect(colorPalette, &QAction::triggered, this, [this](){
        auto dialog = new ColorDialog(this);
        dialog->show();
        dialog->raise();
    });

    auto versionAction = systemMenu->addAction(tr("Version : ") + qApp->applicationVersion());
    versionAction->setEnabled(false);

    this->ui->horizontalLayout_2->setStretch(2, 1);

    openGameProj->setShortcut(Qt::CTRL | Qt::Key_O);
    saveProj->setShortcut(Qt::CTRL | Qt::Key_S);
    quit->setShortcut(Qt::CTRL | Qt::Key_Q);

}

FormTaskBar::~FormTaskBar()
{
    delete ui;

    auto&& settings = ComponentBase::getAppSettings();
    QVariantList recentFiles;
    auto recentProjs = settings.value("recentFiles", recentFiles).toList();

    auto result = std::remove_if(recentProjs.begin(), recentProjs.end(), [](QVariant v){
        auto path = v.toString();
        return QDir(path).exists() == false;
    });
    recentProjs.erase(result, recentProjs.end());
    settings.sync();
}

void FormTaskBar::updateRecentMenu()
{
    recentProjMenu->clear();
    auto&& settings = ComponentBase::getAppSettings();
    QVariantList recentFiles;
    auto recentProjs = settings.value("recentFiles", recentFiles).toList();
    for(auto& proj : recentProjs){
        auto path = proj.toString();
        auto dir = QDir(path);
        auto action = new QAction(dir.dirName() + " (" + path + ")");
        action->setEnabled(dir.exists());

        if(dir.exists()){
            //ディレクトリが見つかりません。プロジェクト終了時に履歴からこの項目を削除します。
            action->setToolTip(tr("Not Found Directory. Delete this item from the history at the end of the project."));
        }

        connect(action, &QAction::triggered, this, [this, path]()
        {
            emit this->requestOpenProj(path);
        });
        recentProjMenu->addAction(action);
    }
}

void FormTaskBar::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    pressLeftButton = event->button() & Qt::LeftButton;
    this->beforeMousePos = event->pos();
}

void FormTaskBar::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
    if(pressLeftButton){
        emit this->dragging(event, event->pos() - this->beforeMousePos);
    }
}

void FormTaskBar::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    pressLeftButton = false;
    beforeMousePos = QPoint();
}

void FormTaskBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
    emit this->doubleClick();
}

void FormTaskBar::SetIconWithReverceColor(QPushButton *button, QString iconPath)
{
    QImage img(iconPath);

    auto&& settings = ComponentBase::getAppSettings();
    auto theme = (Theme)settings.value("appTheme", 2).toInt();
    if(theme == Theme::System)
    {
        QSettings regist("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
        if(regist.value("AppsUseLightTheme")==0){
            theme = Theme::Dark;
        }
        else{
            theme = Theme::Light;
        }
    }
    if(theme == Theme::Dark){
        graphics::ReverceHSVValue(img);
    }
    button->setIcon(QIcon(QPixmap::fromImage(img)));
}

void FormTaskBar::emitChangeTheme(Theme theme)
{
    auto&& settings = ComponentBase::getAppSettings();
    settings.setValue("appTheme", theme);

    SetIconWithReverceColor(this->ui->closeButton, ":images/resources/image/close.svg");
    SetIconWithReverceColor(this->ui->maximumButton, ":images/resources/image/maxminze.svg");
    SetIconWithReverceColor(this->ui->minimumButton, ":images/resources/image/minimize.svg");

    emit this->changeTheme(theme);
}

