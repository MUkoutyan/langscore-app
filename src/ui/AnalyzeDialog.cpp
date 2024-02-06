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

        const auto& outputDirPath = this->setting->analyzeDirectoryPath();
        auto outputDir = QDir(outputDirPath);
        this->ui->analyzeButton->setEnabled(true);
        if(outputDir.exists() == false){
            this->ui->invokeLog->insertPlainText("Analyze Failed.");
            this->ui->invokeLog->insertPlainText(this->invoker->lastProcessOption().join(' '));
            //このテキストが表示される場合、Ci-en等からサポートに問い合わせてください。
            this->ui->invokeLog->insertPlainText(tr("If you see this text, contact support through Ci-en or other means."));
            this->ui->invokeLog->insertPlainText(tr("https://ci-en.net/creator/16302"));
            return;
        }
        this->ui->invokeLog->insertPlainText("Analyze Process Done.");

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

    std::array<std::string, 3> supportedExtention = {
        ".rpgproject",".rvproj2",".rmmzproject"
    };

    auto urls = mimeData->urls();
    for(const auto& url : urls){
        auto fi = QFileInfo(url.toLocalFile());
        if(fi.isFile()){
            auto extention = fi.filesystemFilePath().extension();
            if(std::ranges::find(supportedExtention, extention) != supportedExtention.end()){
                auto dir = fi.dir();
                this->openFile(dir.filesystemAbsolutePath().string().c_str());
            }
        }
        else if(fi.isDir()){
            if(settings::getProjectType(url.toLocalFile()) != settings::ProjectType::None){
                this->openFile(url.toLocalFile());
                return;
            }
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
    if(QFileInfo(gameProjDirPath).isFile()){
        gameProjDirPath = QFileInfo(gameProjDirPath).dir().absolutePath();
    }
    this->setting->setGameProjectPath(gameProjDirPath);
    if(this->setting->projectType == settings::ProjectType::None){
        return;
    }

    //analyzeフォルダがあればスキップ
    if(QFile::exists(this->setting->analyzeDirectoryPath())){
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
