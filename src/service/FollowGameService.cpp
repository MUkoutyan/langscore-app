#include "FollowGameService.h"
#include "EditingTranslationExporter.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QFileSystemWatcher>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

//windows.h のマクロが他のヘッダーとぶつからないよう、最後に読み込む
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

//pid のプロセスが動いていて、writtenAt（プロセスIDを書いた時刻）より前に起動したものなら true。
//プロセスIDは使い回されるため、書いた後に起動したプロセスは別のものとして扱う
static bool IsProcessRunningSince(quint32 pid, const QDateTime& writtenAt)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if(process == nullptr) { return false; }
    DWORD exitCode = 0;
    FILETIME created{}, exited{}, kernel{}, user{};
    const bool running = GetExitCodeProcess(process, &exitCode) 
                      && (exitCode == STILL_ACTIVE)
                      && GetProcessTimes(process, &created, &exited, &kernel, &user);
    CloseHandle(process);
    if(running == false) { return false; }

    //FILETIME は 1601/1/1 からの 100ns 単位
    ULARGE_INTEGER time;
    time.LowPart = created.dwLowDateTime;
    time.HighPart = created.dwHighDateTime;
    const qint64 createdMSecs = static_cast<qint64>((time.QuadPart - 116444736000000000ULL) / 10000);
    //ファイルの時刻の精度の分だけ余裕を持たせる
    return createdMSecs <= writtenAt.toMSecsSinceEpoch() + 2000;
}

FollowGameService::FollowGameService(ComponentBase* t)
    : ComponentBase(t)
    , QObject()
    , remoteHubWatcher(new QFileSystemWatcher(this))
    , readTimer(new QTimer(this))
{
    this->addDispatch(this);

    this->readTimer->setSingleShot(true);
    this->readTimer->setInterval(100);
    connect(this->readTimer, &QTimer::timeout, this, [this]() { this->showGameCurrentText(); });

    auto watchCurrentFile = [this]() {
        const auto currentFile = this->remoteHubDirectoryPath() + "/current.json";
        if(QFile::exists(currentFile) && this->remoteHubWatcher->files().contains(currentFile) == false) {
            this->remoteHubWatcher->addPath(currentFile);
        }
        this->readTimer->start();
    };
    //ファイルの作り直しでも監視が外れるため、フォルダーも監視する。
    connect(this->remoteHubWatcher, &QFileSystemWatcher::directoryChanged, this, watchCurrentFile);
    connect(this->remoteHubWatcher, &QFileSystemWatcher::fileChanged, this, watchCurrentFile);

    //ゲームが起動しているか（テストプレイの開始・終了）を確かめる
    auto* gameCheckTimer = new QTimer(this);
    gameCheckTimer->setInterval(1000);
    connect(gameCheckTimer, &QTimer::timeout, this, [this]() {
        this->dispatch(NotifyGameRunning, {this->isGameRunning()});
    });
    gameCheckTimer->start();
}

FollowGameService::~FollowGameService()
{
    this->removeDispatch(this);
}

void FollowGameService::receive(DispatchType type, const QVariantList& args)
{
    if(type == ComponentBase::FollowGame)
    {
        if(args.empty()) { return; }
        if(args[0].toBool()) {
            this->startFollowGame();
        }
        else {
            this->stopFollowGame();
        }
    }
    else if(type == ComponentBase::ApplyToGame) {
        this->applyToGame();
    }
}

void FollowGameService::startFollowGame()
{
    this->isFollowing = true;
    this->remoteHubWatcher->addPath(this->remoteHubDirectoryPath());
    const auto currentFile = this->remoteHubDirectoryPath() + "/current.json";
    if(QFile::exists(currentFile)) { this->remoteHubWatcher->addPath(currentFile); }
}

void FollowGameService::stopFollowGame()
{
    this->isFollowing = false;
    this->readTimer->stop();
    const auto paths = this->remoteHubWatcher->files() + this->remoteHubWatcher->directories();
    if(paths.isEmpty() == false) { this->remoteHubWatcher->removePaths(paths); }
}

void FollowGameService::showGameCurrentText()
{
    if(this->isFollowing == false) { return; }
    
    QFile file(this->remoteHubDirectoryPath() + "/current.json");
    if(file.open(QIODevice::ReadOnly) == false) { return; }

    //ゲーム側でファイルを書いている場合はfalse.
    //ポーリングなので特に何もせず、次の更新時で読めるかを確認する。
    const auto doc = QJsonDocument::fromJson(file.readAll());
    if(doc.isObject() == false) { return; }

    const auto texts = doc.object()["texts"].toArray();
    if(texts.isEmpty()) { return; }

    //翻訳ファイルが分かる文章のうち、カーソルの当たっている選択肢を選ぶ。
    //無ければ最後に表示した文章を選ぶ。カーソルの無い選択肢よりはメッセージを優先する
    auto shown = texts.last().toObject();
    int shownPriority = -1;
    for(const auto& value : texts)
    {
        const auto text = value.toObject();
        if(text["file"].toString().isEmpty()) {
            continue;
        }

        int priority = 0;
        if(text["selected"].toBool()) {
            priority = 2;
        }
        else if(text["choice"].toBool() == false) {
            priority = 1;
        }

        if(shownPriority <= priority) {
            shown = text;
            shownPriority = priority;
        }
    }
    this->dispatch(ShowGameText, {shown["original"].toString(), shown["file"].toString(), doc.object()["language"].toString()});
}

void FollowGameService::applyToGame()
{
    //反映した内容とプロジェクトが食い違わないよう、反映する前に必ず保存する
    this->dispatch(SaveProject, {});
    const auto files = langscore::UnexportedEditingFiles(*this->setting);
    if(files.isEmpty()) {
        this->dispatch(NotifyApplyToGame, {0});
        return;
    }
    //VXAceでは書き出したCSVをパッキング後に消しているため、divisiで書き出し直してからパッキングする
    if(this->setting->projectType == settings::VXAce) {
        this->dispatch(ExportForGame, {});
        return;
    }
    const auto updatedFiles = langscore::ApplyEditingTranslations(*this->setting, files);
    this->dispatch(NotifyApplyToGame, {updatedFiles.size()});
}

QString FollowGameService::remoteHubDirectoryPath() const
{
    return this->setting->langscoreProjectDirectory + "/remote";
}

//Langscore_RemoteHub は起動時に自分のプロセスIDを pid に書く。そのプロセスが動いていれば起動中
bool FollowGameService::isGameRunning() const
{
    if(this->setting->gameProjectPath.isEmpty()) { return false; }
    const QFileInfo pidFile(this->remoteHubDirectoryPath() + "/pid");
    QFile file(pidFile.absoluteFilePath());
    if(file.open(QIODevice::ReadOnly) == false) { return false; }
    bool isOk = false;
    const auto pid = file.readAll().trimmed().toUInt(&isOk);
    if(isOk == false) { return false; }
    return IsProcessRunningSince(pid, pidFile.lastModified());
}
