#include "MainComponent.h"
#include "WriteModeComponent.h"
#include "src/invoker.h"
#include "ui_MainComponent.h"
#include "ui_WriteModeComponent.h"

#include "../csv.hpp"

#include <QPaintEvent>
#include <QDropEvent>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>

#include <QDebug>

using namespace langscore;

const static QMap<QString, settings::ProjectType> projectExtensionAndType = {
    {"rvproj2",     settings::VXAce},
    {"rpgproject",  settings::MV},
    {"rmmzproject", settings::MZ},
};

MainComponent::MainComponent(ComponentBase *setting, QWidget *parent) :
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
        this->setting->tempFileOutputDirectory = emit this->requestOpenOutputDir(this->setting->gameProjectPath);
    });

    this->ui->invokeLog->setVisible(false);
}

MainComponent::~MainComponent()
{
    delete ui;
}

void MainComponent::openGameProject(QString path){
    auto fileInfo = findGameProject({QUrl::fromLocalFile(path)});
    this->openFiles(fileInfo);
}

void MainComponent::dropEvent(QDropEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }
    auto fileInfo = findGameProject(mimeData->urls());
    if(fileInfo.first != ""){
        openFiles(fileInfo);
    }
}

void MainComponent::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData* mimeData = event->mimeData();
    //URLがあれば通す
    if(mimeData->hasUrls() == false){ return; }

    QList<QUrl> urlList = mimeData->urls();
    if(findGameProject(std::move(urlList)).first != ""){
        event->acceptProposedAction();
    }
}

bool MainComponent::event(QEvent *event)
{
    return QWidget::event(event);
}

void MainComponent::openFiles(std::pair<QString, settings::ProjectType> fileInfo)
{
    this->history->clear();

    auto path = fileInfo.first;
    this->setting->projectType = fileInfo.second;
    this->setting->gameProjectPath = path;
    this->setting->tempFileOutputDirectory = path + "/langscore_proj";
    if(QFile::exists(this->setting->tempFileDirectoryPath()))
    {
        auto projFile = this->setting->tempFileOutputDirectory+"/config.json";
        if(QFile::exists(projFile))
        {
            this->setting->load(projFile);
        }
        toWriteMode();
    }
    else{
        toAnalyzeMode();
    }

    this->update();
}

std::pair<QString, settings::ProjectType> MainComponent::findGameProject(QList<QUrl> urlList)
{
    for(auto& url : urlList)
    {
        auto filePath = url.toLocalFile();
        QFileInfo info(filePath);
        if(info.isFile() && projectExtensionAndType.contains(info.suffix())){
            return {filePath, projectExtensionAndType[info.suffix()]};
        }
        else if(info.isDir()){
            QDir dir(filePath);
            auto files = dir.entryInfoList();
            if(files.empty()){ continue; }

            for(auto& child : files){
                if(projectExtensionAndType.contains(child.suffix())){
                    return {filePath, projectExtensionAndType[info.suffix()]};
                }
            }
        }
    }

    return {"", settings::VXAce};
}

void MainComponent::toAnalyzeMode()
{
    this->ui->analyzeButton->setEnabled(true);
    this->ui->setOutputDirButton->setEnabled(true);
    this->ui->label->setVisible(false);
    this->ui->lineEdit->setText(this->setting->tempFileOutputDirectory);
    this->ui->lineEdit->setReadOnly(false);
    this->ui->invokeLog->setVisible(true);

    this->ui->base->setCurrentIndex(0);
}

void MainComponent::toWriteMode()
{
    auto&& settings = ComponentBase::getAppSettings();
    auto projDir = this->setting->gameProjectPath;
    QVariantList recentFiles;
    auto recentList = settings.value("recentFiles", recentFiles).toList();
    if(recentList.size() >= 8){
        recentList.remove(7, qMin(1, recentList.size()-7));
    }
    int index = 0;
    for(auto& obj : recentList){
        if(obj.toString() == projDir){
            recentList.removeAt(index);
            break;
        }
        ++index;
    }
    recentList.insert(0, QVariant(projDir));
    settings.setValue("recentFiles", recentList);
    settings.sync();

    this->writeUi->show();
    this->ui->base->setCurrentIndex(1);

    emit this->openProject();
}

void MainComponent::invokeAnalyze()
{
    invoker invoker(this);

    connect(&invoker, &invoker::getStdOut, this, [this](QString text){
        this->ui->invokeLog->insertPlainText(text);
        this->update();
    });

    if(invoker.analyze() == false){ return; }

    if(QFile::exists(this->setting->tempFileDirectoryPath()) == false){ return; }

    auto writedScripts = [this]()
    {
        const auto tempFolder = this->setting->tempFileDirectoryPath();
        auto scriptCsv     = readCsv(tempFolder + "Scripts.csv");
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

        auto& scriptList = this->setting->writeObj.scriptInfo;
        const auto IsIgnoreText = [&scriptList](QString fileName, size_t row, size_t col){
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x){
                return x.name == fileName &&  std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), std::pair<size_t, size_t>{row, col}) != x.ignorePoint.cend();
            });
            return result != scriptList.cend();
        };
        if(IsIgnoreText(fileName, row, col)){
            settings::TextPoint txtPos;
            txtPos.row = row;
            txtPos.col = col;
            auto info = settings::ScriptInfo{{fileName, false, 0}, {txtPos}};
            this->setting->writeObj.scriptInfo.emplace_back(std::move(info));
        }
    }

    this->toWriteMode();
}
