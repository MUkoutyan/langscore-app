#include "AnalyzeDialog.h"
#include "src/invoker.h"
#include "ui_AnalyzeDialog.h"

#include <QScrollBar>
#include <QPaintEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QFileInfo>

#include <filesystem>

#include <QDebug>

AnalyzeDialog::AnalyzeDialog(ComponentBase *setting, QWidget *parent) :
    QDialog(parent),
    ComponentBase(setting),
    ui(new Ui::AnalyzeDialog()),
    invoker(new class invoker(this))
{
    this->ui->setupUi(this);
    this->setAcceptDrops(true); //D&D許可
    this->setWindowFlag(Qt::FramelessWindowHint);

    this->setObjectName("analyzeDialog");
    this->setStyleSheet("#analyzeDialog{border: 2px solid #3f3f3f;}");

    connect(this->ui->analyzeButton, &QPushButton::clicked, this, &AnalyzeDialog::invokeAnalyze);

    this->ui->invokeLog->setVisible(false);

    connect(invoker, &invoker::getStdOut, this, [this](QString text){
        text.replace("\r\n", "\n");
        this->ui->invokeLog->insertPlainText(text);
        auto vBar = this->ui->invokeLog->verticalScrollBar();
        vBar->setValue(vBar->maximum());
        this->update();
    });
    connect(invoker, &invoker::update, this, [this](){
        this->update();
    });

    connect(invoker, &invoker::finish, this, [this](int exitCode)
    {
        if(exitCode != invoker::SUCCESS){ return; }

        const auto& outputDirPath = this->setting->tempFileDirectoryPath();
        auto outputDir = QDir(outputDirPath);
        if(outputDir.exists() == false){ return; }
        this->ui->analyzeButton->setEnabled(true);

        emit this->toWriteMode(this->setting->gameProjectPath);
    });
}

AnalyzeDialog::~AnalyzeDialog()
{
    delete ui;
}

void AnalyzeDialog::moveParent(QPoint delta)
{
    QPoint pos(this->pos().x() + delta.x(), this->pos().y() + delta.y());
    move(pos);
}

void AnalyzeDialog::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }
    auto urls = mimeData->urls();
    for(const auto& url : urls){
        if(settings::getProjectType(url.toLocalFile()) != settings::ProjectType::None){
            this->openFile(url.toLocalFile());
            return;
        }
    }
}

void AnalyzeDialog::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();
    for(const auto& url : urlList){
        if(settings::getProjectType(url.toLocalFile()) != settings::ProjectType::None){
            event->acceptProposedAction();
            return;
        }
    }
}

void AnalyzeDialog::openFile(QString gameProjDirPath)
{
    this->setting->setGameProjectPath(gameProjDirPath);
    if(this->setting->projectType == settings::ProjectType::None){
        return;
    }

    if(QFile::exists(this->setting->tempFileDirectoryPath())){
        emit this->toWriteMode(gameProjDirPath);
    }
    else{
        emit this->toAnalyzeMode();
        this->ui->analyzeButton->setEnabled(true);
        this->ui->label->setVisible(false);
        this->ui->lineEdit->setText(this->setting->langscoreProjectDirectory);
        this->ui->lineEdit->setReadOnly(false);
        this->ui->invokeLog->setVisible(true);
    }
}

void AnalyzeDialog::invokeAnalyze()
{
    this->ui->analyzeButton->setEnabled(false);
    this->dispatch(DispatchType::SaveProject,{});
    this->ui->invokeLog->clear();
    invoker->analyze();
}
