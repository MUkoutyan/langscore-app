#include "FormTaskBar.h"
#include "ui_FormTaskBar.h"
#include "ComponentBase.h"
#include "../graphics.hpp"

#include <QMenuBar>
#include <QMouseEvent>
#include <QFile>
#include <QDir>


FormTaskBar::FormTaskBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FormTaskBar)
    , pressLeftButton(false)
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

    const auto SetIcon = [](QPushButton* button, QString iconPath){
        QImage img(iconPath);
        graphics::ReverceHSVValue(img);
        button->setIcon(QIcon(QPixmap::fromImage(img)));
    };
    SetIcon(this->ui->closeButton, ":images/resources/image/close.svg");
    SetIcon(this->ui->maximumButton, ":images/resources/image/maxminze.svg");
    SetIcon(this->ui->minimumButton, ":images/resources/image/minimize.svg");

    connect(this->ui->closeButton,   &QPushButton::clicked, this, &FormTaskBar::pushClose);
    connect(this->ui->maximumButton, &QPushButton::clicked, this, [this, &SetIcon](){
        if(emit this->maximum()){
            SetIcon(this->ui->maximumButton, ":/images/resources/image/normalize.svg");
        }
        else{
            SetIcon(this->ui->maximumButton, ":/images/resources/image/maxminze.svg");
        }
    });
    connect(this->ui->minimumButton, &QPushButton::clicked, this, &FormTaskBar::minimum);

    auto menuBar = new QMenuBar(this);
    this->ui->horizontalLayout_2->insertWidget(0, menuBar);
    auto fileMenu = menuBar->addMenu(tr("File"));
    auto openGameProj = fileMenu->addAction(tr("Open Game Project..."));
    auto saveProj     = fileMenu->addAction(tr("Save Langscore Project..."));
    //auto saveAsProj   = fileMenu->addAction(tr("Save as Langscore Project..."));
    auto recentProjMenu = fileMenu->addMenu(tr("Recent Project"));

    auto&& settings = ComponentBase::getSettings();
    QVariantList recentFiles;
    auto recentProjs = settings.value("recentFiles", recentFiles).toList();
    for(auto& proj : recentProjs){
        auto path = proj.toString();
        auto action = new QAction(QDir(path).dirName() + " (" + path + ")");
        connect(action, &QAction::triggered, this, [this, path](){
            emit this->requestOpenProj(path);
        });
        recentProjMenu->addAction(action);
    }

    auto quit = fileMenu->addAction(tr("Quit"));

    connect(openGameProj, &QAction::triggered, this, &FormTaskBar::openGameProj);
    connect(saveProj,     &QAction::triggered, this, &FormTaskBar::saveProj);
//    connect(saveAsProj,   &QAction::triggered, this, &FormTaskBar::saveAsProj);
    connect(quit,         &QAction::triggered, this, &FormTaskBar::quit);

    auto helpMenu = menuBar->addMenu(tr("Help"));

    this->ui->horizontalLayout_2->setStretch(2, 1);

    openGameProj->setShortcut(Qt::CTRL | Qt::Key_O);
    saveProj->setShortcut(Qt::CTRL | Qt::Key_S);
    quit->setShortcut(Qt::CTRL | Qt::Key_Q);

}

FormTaskBar::~FormTaskBar()
{
    delete ui;
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

