#include "AnalyzeDialog.h"
#include "src/invoker.h"
#include "src/service/EditingTranslationExporter.h"
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
    //ゲームへの変更の説明とプラグインの設定は、プロジェクトのドロップ後に表示する
    this->ui->startNoticeLabel->setVisible(false);
    this->ui->pluginOptionGroup->setVisible(false);

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

    connect(invoker, &invoker::finish, this, [this](int exitCode, QProcess::ExitStatus status)
    {
        const auto step = this->invokeStep;
        this->invokeStep = InvokeStep::None;
        if(exitCode != invoker::SUCCESS || status == QProcess::CrashExit){
            this->ui->analyzeButton->setEnabled(true);
            if(step == InvokeStep::Analyze) {
                this->ui->invokeLog->insertPlainText("Analyze Failed.");
            }
            else {
                this->ui->invokeLog->insertPlainText("Failed.");
            }
            return;
        }

        if(step == InvokeStep::Analyze)
        {
            const auto& outputDirPath = this->setting->analyzeDirectoryPath();
            auto outputDir = QDir(outputDirPath);
            if(outputDir.exists() == false){
                this->ui->analyzeButton->setEnabled(true);
                this->ui->invokeLog->insertPlainText("Analyze Failed.");
                this->ui->invokeLog->insertPlainText(this->invoker->lastProcessOption().join(' '));
                //このテキストが表示される場合、Ci-en等からサポートに問い合わせてください。
                this->ui->invokeLog->insertPlainText(tr("If you see this text, contact support through Ci-en or other means."));
                this->ui->invokeLog->insertPlainText(tr("https://ci-en.net/creator/16302"));
                return;
            }
            this->ui->invokeLog->insertPlainText("Analyze Process Done.\n");
            this->exportToGame();
            return;
        }

        //VXAce は翻訳CSVをパッキングしないとゲームで使えない
        if(step == InvokeStep::Export && this->setting->projectType == settings::VXAce)
        {
            this->invokeStep = InvokeStep::Packing;
            this->invoker->packing();
            return;
        }
        if(step == InvokeStep::Packing) {
            //ゲームは .rvdata2 を読むため、パッキング済みのCSVは残さない
            langscore::RemovePackedCsvFiles(*this->setting);
        }
        this->finishStartTranslation();
    });

    {
        auto&& settings = ComponentBase::getAppSettings();
        QVariantList recentFiles;
        auto recentProjs = settings.value("recentFiles", recentFiles).toList();
        for(auto& proj : recentProjs) 
        {
            auto path = proj.toString();
            auto dir = QDir(path);

            if(dir.exists() == false) {
                continue;
            }

            auto treeItem = new QTreeWidgetItem();
            treeItem->setSizeHint(0, QSize{-1, 80});
            treeItem->setText(0, dir.dirName() + " (" + dir.absolutePath() + ")");
            treeItem->setData(0, Qt::UserRole, dir.absolutePath());
            
            this->ui->recentProjectTree->addTopLevelItem(treeItem);
        }

        connect(this->ui->recentProjectTree, &QTreeWidget::doubleClicked, this, [this](const QModelIndex& index) {
            auto path = index.data(Qt::UserRole).toString();
            emit this->openFile(path);
        });
    }
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
            //std::filesystem::path::string() は ANSI コードページに変換され日本語パスが化けるため Qt でパスを扱う
            const auto extention = "." + fi.suffix().toStdString();
            if(std::ranges::find(supportedExtention, extention) != supportedExtention.end()){
                this->openFile(fi.absolutePath());
            }
        }
        else if(fi.isDir()){
            if(settings::getProjectType(url.toLocalFile()) != settings::ProjectType::None){
                this->openFile(url.toLocalFile());
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

    //analyzeフォルダとlsjsonファイルが両方あればスキップ
    QString analyzeDirPath = this->setting->analyzeDirectoryPath();
    QString lsJsonFilePath = analyzeDirPath + "/*.lsjson";  
    QDir dir(analyzeDirPath);  
    QStringList lsJsonFiles = dir.entryList(QStringList() << "*.lsjson", QDir::Files);  
    if (QFile::exists(analyzeDirPath) && lsJsonFiles.isEmpty() == false) {
        emit this->toWriteMode(gameProjDirPath);
    }
    else 
    {
        //以前は解析結果を .csv で保存していたため、.lsjson が無ければ強制的に再解析する
        emit this->toAnalyzeMode();
        this->ui->analyzeButton->setEnabled(true);
        this->ui->label->setVisible(false);
        this->ui->lineEdit->setText(this->setting->langscoreProjectDirectory);
        this->ui->lineEdit->setReadOnly(false);
        this->ui->invokeLog->setVisible(true);
        //翻訳CSVの書き出し先のフォルダ名はプロジェクトの種類で異なる。VXAce は Data/Translate、MV・MZ は data/translate
        const QFileInfo translateDir(this->setting->translateDirectoryPath());
        //翻訳作業を開始すると、ゲーム側に以下の変更を行います。
        //・Langscoreプラグインを自動で追加します。
        //・%1フォルダに%2フォルダを作成し、翻訳CSVを書き出します。
        this->ui->startNoticeLabel->setText(tr("When you start translating, the following changes are made to the game.\n"
                                               "* The Langscore plugin is added automatically.\n"
                                               "* A %2 folder is created in the %1 folder, and the translation CSV files are exported to it.")
                                            .arg(translateDir.dir().dirName(), translateDir.fileName()));
        this->ui->startNoticeLabel->setVisible(true);
        this->ui->pluginOptionGroup->setVisible(true);
        this->ui->recentProjectTree->setVisible(false);
        this->ui->recentProjectLabel->setVisible(false);
    }
}

void AnalyzeDialog::invokeAnalyze()
{
    this->setting->defaultLanguage = settings::getLowerBcp47Name(QLocale::system());
    //プラグインの書き出しで使うため、解析の前にプロジェクトへ保存する
    this->setting->writeObj.enableLanguagePatch = this->ui->enablePatch->isChecked();
    this->setting->writeObj.enableTranslateDefLang = this->ui->enableTransDefLang->isChecked();
    this->setting->writeObj.enableRemoteHub = this->ui->enableRemoteHub->isChecked();
    this->ui->analyzeButton->setEnabled(false);
    //SaveProject を dispatch すると、編集画面に残っている前のプロジェクトの表まで保存されるため、設定のみ保存する
    this->setting->saveForProject();
    this->ui->invokeLog->clear();
    this->invokeStep = InvokeStep::Analyze;
    invoker->analyze();
}

//解析の結果を読み込み、言語を設定してから、ゲームにプラグインと翻訳CSVを書き出す
void AnalyzeDialog::exportToGame()
{
    const auto configPath = this->setting->langscoreProjectDirectory + "/config.json";
    this->setting->load(configPath);
    this->setting->setupLanguages(settings::supportedLocales());
    this->setting->useGameTranslateFolderForExport();
    this->setting->setPackingDirectory(this->setting->writeObj.exportDirectory);
    this->setting->saveForProject();

    this->ui->invokeLog->insertPlainText(tr("Adding plugin and writing translation files...\n"));
    this->invokeStep = InvokeStep::Export;
    this->invoker->exportFirstTime();
}

void AnalyzeDialog::finishStartTranslation()
{
    this->ui->invokeLog->insertPlainText("Done.");
    this->setting->isPluginRegistered = true;
    this->setting->isFirstExported = true;
    this->setting->saveForProject();

    emit this->translationStarted();
    emit this->toWriteMode(this->setting->gameProjectPath);
}
