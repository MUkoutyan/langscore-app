#include "MainComponent.h"
#include "WriteModeComponent.h"
#include "ui_MainComponent.h"
#include "ui_WriteModeComponent.h"

#include "../invoker.h"

#include <QPaintEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QFileInfo>

#include <QDebug>

MainComponent::MainComponent(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::MainComponent()),
    writeUi(new WriteModeComponent(this))
{
    this->setAcceptDrops(true); //D&D許可

    this->ui->setupUi(this);
    this->ui->base->addWidget(writeUi);

    connect(this->ui->analyzeButton, &QPushButton::clicked, this, &MainComponent::invokeAnalyze);

    connect(this->ui->setOutputDirButton, &QPushButton::clicked, this, [this]()
    {
        this->setting.outputDirectory = emit this->requestOpenOutputDir(setting.project);
    });

    this->ui->invokeLog->setVisible(false);
}

MainComponent::~MainComponent()
{
    delete ui;
}

void MainComponent::paintEvent(QPaintEvent *p)
{
    if(this->isHidden()){ return; }


}

void MainComponent::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }
    auto filePath = findGameProject(mimeData->urls());
    if(filePath != ""){
        openFiles(filePath);
    }
}

void MainComponent::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();
    if(findGameProject(std::move(urlList)) != ""){
        event->acceptProposedAction();
    }
}

bool MainComponent::event(QEvent *event)
{
    return QWidget::event(event);
}

void MainComponent::openFiles(QString path)
{
    setting.project = path;
    setting.outputDirectory = path + "/langscore_proj";
    if(QFile::exists(setting.outputDirectory+"/tmp")){
        toWriteMode();
    }
    else{
        toAnalyzeMode();
    }

    this->update();
}

QString MainComponent::findGameProject(QList<QUrl> urlList)
{
    for(auto& url : urlList)
    {
        auto filePath = url.toLocalFile();
        QFileInfo info(filePath);
        if(info.isFile() && info.suffix() == "rvproj2"){
            return filePath;
        }
        else if(info.isDir()){
            QDir dir(filePath);
            auto files = dir.entryInfoList();
            if(files.empty()){ continue; }

            for(auto& child : files){
                if(child.suffix() == "rvproj2"){
                    return filePath;
                }
            }
        }
    }

    return "";
}

void MainComponent::toAnalyzeMode()
{
    this->ui->analyzeButton->setEnabled(true);
    this->ui->setOutputDirButton->setEnabled(true);
    this->ui->label->setVisible(false);
    this->ui->lineEdit->setText(setting.outputDirectory);
    this->ui->lineEdit->setReadOnly(false);
    this->ui->invokeLog->setVisible(true);

    this->ui->base->setCurrentIndex(0);
}

void MainComponent::toWriteMode()
{
    this->writeUi->show();
    this->ui->base->setCurrentIndex(1);
}

void MainComponent::invokeAnalyze()
{
    invoker invoker(this->setting);

    connect(&invoker, &invoker::getStdOut, this, [this](QString text){
        this->ui->invokeLog->insertPlainText(text);
    });

    if(invoker.run() == false){ return; }

    if(QFile::exists(setting.outputDirectory+"/tmp")){
        this->toWriteMode();
    }
}
