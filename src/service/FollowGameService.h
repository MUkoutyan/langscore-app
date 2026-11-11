#pragma once

#include <QObject>
#include <QString>
#include "ComponentBase.h"

class QFileSystemWatcher;
class QTimer;

//テストプレイ中のゲーム連携
//・ゲームが起動しているかを1秒ごとに確認し、NotifyGameRunning で通知する
//・FollowGame で追従している間は、ゲームで表示している文章を ShowGameText で通知する
//・ApplyToGame で、編集した翻訳をゲームの翻訳ファイルに反映する
class FollowGameService : public ComponentBase, public QObject
{
public:

    FollowGameService(ComponentBase* t);
    ~FollowGameService() override;

    void receive(DispatchType type, const QVariantList& args) override;

    //Langscore_RemoteHub の入ったゲームが起動しているか
    bool isGameRunning() const;
    QString remoteHubDirectoryPath() const;

private:

    //Remoteプラグインが書き出す、表示中の文章のファイルを監視する
    void startFollowGame();
    void stopFollowGame();
    //ゲームが書き出した表示中の文章から、エディターでハイライトする文章を選ぶ
    void showGameCurrentText();
    //編集した翻訳文をゲーム側に反映する。RemoteHubが入っていればゲームが自動で読み直す。
    void applyToGame();

    bool isFollowing = false;
    QFileSystemWatcher* remoteHubWatcher;
    QTimer* readTimer;
};
