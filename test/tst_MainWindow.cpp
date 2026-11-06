#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QTimer>
#include <QApplication>
#include <QPushButton>
#include <QMessageBox>
#include <QCloseEvent>
#include <QUndoCommand>
#include "MainWindow.h"

// 履歴と dispatch を使うために公開する
struct TestMainWindow : public MainWindow
{
    using ComponentBase::history;
    using ComponentBase::dispatch;
    using ComponentBase::setting;
};

namespace
{
    //閉じるときに確認のダイアログが出たかを記録し、出たらキャンセルする
    bool CloseAndCheckAsked(TestMainWindow& window, bool& accepted)
    {
        bool asked = false;
        QTimer::singleShot(0, [&asked]() {
            if(auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                asked = true;
                box->button(QMessageBox::Cancel)->click();
            }
        });
        QCloseEvent event;
        window.closeEvent(&event);
        accepted = event.isAccepted();
        return asked;
    }
}

TEST(MainWindowTest, AskSaveOnlyWhenEdited)
{
    // 編集した後に閉じると保存を確認し、保存した後は確認しないこと
    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    QDir().mkpath(gameDir);
    QFile projectFile(gameDir + "/Game.rmmzproject");
    ASSERT_TRUE(projectFile.open(QFile::WriteOnly));
    projectFile.close();

    TestMainWindow window;
    window.setting->setGameProjectPath(gameDir);
    QDir().mkpath(window.setting->langscoreProjectDirectory);

    window.history->push(new QUndoCommand("edit"));
    bool accepted = true;
    EXPECT_TRUE(CloseAndCheckAsked(window, accepted)) << "編集したのに保存を確認していません";
    EXPECT_FALSE(accepted) << "キャンセルしたのに閉じています";

    window.dispatch(ComponentBase::SaveProject, {});
    EXPECT_FALSE(CloseAndCheckAsked(window, accepted)) << "保存したのに確認しています";
    EXPECT_TRUE(accepted);
}
