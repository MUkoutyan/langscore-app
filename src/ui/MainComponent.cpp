#include "MainComponent.h"
#include "WriteModeComponent.h"
#include "ui_MainComponent.h"
#include "ui_WriteModeComponent.h"

#include "../csv.hpp"
#include "../invoker.h"

#include <QPaintEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QFileInfo>

#include <QDebug>

using namespace langscore;

MainComponent::MainComponent(Common::Type setting, QWidget *parent) :
    QWidget(parent),
    ComponentBase(setting),
    ui(new Ui::MainComponent()),
    writeUi(new WriteModeComponent(setting, this))
{
    this->setAcceptDrops(true); //D&D許可

    this->ui->setupUi(this);
    this->ui->base->addWidget(writeUi);

    connect(this->ui->analyzeButton, &QPushButton::clicked, this, &MainComponent::invokeAnalyze);

    connect(this->ui->setOutputDirButton, &QPushButton::clicked, this, [this]()
    {
        this->common.obj->tempFileOutputDirectory = emit this->requestOpenOutputDir(common.obj->gameProjectPath);
    });

    this->ui->invokeLog->setVisible(false);
}

MainComponent::~MainComponent()
{
    delete ui;
}

void MainComponent::openGameProject(QString path){
    this->openFiles(path);
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
    this->common.obj->gameProjectPath = path;
    this->common.obj->tempFileOutputDirectory = path + "/langscore_proj";
    if(QFile::exists(this->common.obj->tempFileDirectoryPath())){
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
    this->ui->lineEdit->setText(this->common.obj->tempFileOutputDirectory);
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
    invoker invoker(this->common.obj);

    connect(&invoker, &invoker::getStdOut, this, [this](QString text){
        this->ui->invokeLog->insertPlainText(text);
    });

    if(invoker.run() == false){ return; }

    if(QFile::exists(this->common.obj->tempFileDirectoryPath()) == false){ return; }

    auto writedScripts = [this]()
    {
        const auto translateFolder = this->common.obj->translateDirectoryPath();
        auto scriptCsv     = readCsv(translateFolder + "Scripts.csv");
        QStringList result;
        scriptCsv.erase(scriptCsv.begin()); //Headerを削除
        for(auto& line : scriptCsv){
            result.emplace_back(line[0]);
        }
        result.sort();
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }();

    for(auto& script : writedScripts)
    {
        auto [fileName, row, col] = parseScriptNameWithRowCol(script);

        auto& scriptList = this->common.obj->writeObj.ignoreScriptInfo;
        const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
                return x.name == fileName &&  std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
            });
            return result != scriptList.cend();
        };
        if(IsIgnoreText(fileName, row, col)){
            this->common.obj->writeObj.ignoreScriptInfo.emplace_back(
                settings::WriteProps::ScriptInfo{fileName, {{size_t(row), size_t(col)}}, 0, false}
            );
        }
    }

    this->toWriteMode();
}
