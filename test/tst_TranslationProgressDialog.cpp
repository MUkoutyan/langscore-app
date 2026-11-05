#include "test_common.h"
#include <QLabel>
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
