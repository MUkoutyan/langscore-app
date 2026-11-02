#include <QTest>
#include <QSignalSpy>
#include <QFile>
#include "invoker.h"

class TestInvoker : public QObject
{
    Q_OBJECT

private slots:
    // 不具合の再発防止
    void testFinishEmittedWhenStartFails();
};

void TestInvoker::testFinishEmittedWhenStartFails()
{
    // divisi.exe を起動できない場合も finish が通知され、呼び出し側が UI を元に戻せること
    QVERIFY(QFile::exists(QCoreApplication::applicationDirPath() + "/bin/divisi.exe") == false);

    ComponentBase component;
    invoker inv(&component);
    QSignalSpy finishSpy(&inv, &invoker::finish);
    inv.analyze();
    QTRY_COMPARE_WITH_TIMEOUT(finishSpy.count(), 1, 1000);
}

QTEST_MAIN(TestInvoker)
#include "tst_Invoker.moc"
