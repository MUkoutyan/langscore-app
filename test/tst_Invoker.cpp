#include "test_common.h"
#include <QFile>
#include "invoker.h"

// 不具合の再発防止
TEST(Invoker, FinishEmittedWhenStartFails)
{
    // divisi.exe を起動できない場合も finish が通知され、呼び出し側が UI を元に戻せること
    ASSERT_FALSE(QFile::exists(QCoreApplication::applicationDirPath() + "/bin/divisi.exe"));

    ComponentBase component;
    invoker inv(&component);
    int finishCount = 0;
    QObject::connect(&inv, &invoker::finish, [&finishCount](int, QProcess::ExitStatus) {
        ++finishCount;
    });
    inv.analyze();

    // 非同期で通知されるため、イベントループを回して待つ
    EXPECT_TRUE(WaitUntil([&finishCount]() { return 0 < finishCount; }, 1000));
    EXPECT_EQ(finishCount, 1);
}
