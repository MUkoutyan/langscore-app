#include "test_common.h"
#include <QLabel>
#include <QPlainTextEdit>
#include <QEventLoop>
#include <QTimer>
#include "dialog/TranslationProgressDialog.h"

TEST(TranslationProgressDialogTest, ColorResetOnNextTranslation)
{
    // 一部失敗した色（黄色）が、次の翻訳を始めたときに残らないこと
    TranslationProgressDialog dialog;
    auto* statusLabel = dialog.findChild<QLabel*>("statusLabel");
    ASSERT_NE(statusLabel, nullptr);
    dialog.startProgress(2);
    dialog.showCompletion(1, 2, "Too Many Requests");
    EXPECT_FALSE(statusLabel->styleSheet().isEmpty());
    EXPECT_TRUE(statusLabel->text().contains("Too Many Requests"));

    dialog.startProgress(2);
    EXPECT_TRUE(statusLabel->styleSheet().isEmpty());
}

namespace {
    //自動で閉じるタイマー（1秒）が動くまで待つ
    void WaitAutoClose()
    {
        QEventLoop loop;
        QTimer::singleShot(1200, &loop, &QEventLoop::quit);
        loop.exec();
    }
}

TEST(TranslationProgressDialogTest, AutoCloseWithoutMessages)
{
    // エラー・警告が無ければ、完了後に自動で閉じること
    TranslationProgressDialog dialog;
    dialog.startProgress(2);
    dialog.show();
    dialog.showCompletion(2, 2);
    WaitAutoClose();
    EXPECT_FALSE(dialog.isVisible());
}

TEST(TranslationProgressDialogTest, KeepOpenWithMessages)
{
    // 警告があれば、すべて成功しても閉じずに内容を表示すること
    TranslationProgressDialog dialog;
    auto* messageText = dialog.findChild<QPlainTextEdit*>("messageText");
    ASSERT_NE(messageText, nullptr);
    dialog.startProgress(2);
    dialog.show();
    dialog.addMessage("[Warning] Row 1 (en): empty");
    dialog.showCompletion(2, 2);
    WaitAutoClose();
    EXPECT_TRUE(dialog.isVisible());
    EXPECT_TRUE(messageText->isVisible());
    EXPECT_TRUE(messageText->toPlainText().contains("empty"));

    // 次の翻訳では前回の内容を消すこと
    dialog.startProgress(1);
    EXPECT_FALSE(dialog.hasMessages());
    EXPECT_FALSE(messageText->isVisible());
}
