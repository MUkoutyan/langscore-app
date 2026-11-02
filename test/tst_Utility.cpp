#include <QTest>
// utility.hpp は settings::ProjectType を直接参照するため、先に settings.h が必要
#include "settings.h"
#include "utility.hpp"

class TestUtility : public QObject
{
    Q_OBJECT

private slots:
    void testWordCountAscii();
    void testWordCountJapanese();
    void testWordCountEmpty();
    void testWordCountMixed();
    void testGetExtensionNormal();
    void testGetExtensionNoExt();
    void testGetExtensionMultiple();
    void testWithoutExtensionNormal();
    void testWithoutExtensionNoExt();
    void testWithoutAllExtensionSingle();
    void testWithoutAllExtensionMultiple();
    void testWithoutAllExtensionNoExt();
    void testGetScriptExtensionVXAce();
    void testGetScriptExtensionMV();
    void testGetScriptExtensionMZ();
};

void TestUtility::testWordCountAscii()
{
    QCOMPARE(langscore::wordCountUTF8("hello"), 5);
}

void TestUtility::testWordCountJapanese()
{
    // 日本語文字（BMP）は toLatin1() が 0 になり、カウント対象となる
    QCOMPARE(langscore::wordCountUTF8(QString::fromUtf8("あいう")), 3);
}

void TestUtility::testWordCountEmpty()
{
    QCOMPARE(langscore::wordCountUTF8(""), 0);
}

void TestUtility::testWordCountMixed()
{
    QCOMPARE(langscore::wordCountUTF8(QString("a") + QString::fromUtf8("あ")), 2);
}

void TestUtility::testGetExtensionNormal()
{
    QCOMPARE(langscore::getExtension("file.txt"), QString("txt"));
}

void TestUtility::testGetExtensionNoExt()
{
    QCOMPARE(langscore::getExtension("file"), QString());
}

void TestUtility::testGetExtensionMultiple()
{
    QCOMPARE(langscore::getExtension("archive.tar.gz"), QString("gz"));
}

void TestUtility::testWithoutExtensionNormal()
{
    QCOMPARE(langscore::withoutExtension("file.txt"), QString("file"));
}

void TestUtility::testWithoutExtensionNoExt()
{
    QCOMPARE(langscore::withoutExtension("file"), QString("file"));
}

void TestUtility::testWithoutAllExtensionSingle()
{
    QCOMPARE(langscore::withoutAllExtension("file.txt"), QString("file"));
}

void TestUtility::testWithoutAllExtensionMultiple()
{
    QCOMPARE(langscore::withoutAllExtension("archive.tar.gz"), QString("archive"));
}

void TestUtility::testWithoutAllExtensionNoExt()
{
    QCOMPARE(langscore::withoutAllExtension("file"), QString("file"));
}

void TestUtility::testGetScriptExtensionVXAce()
{
    QCOMPARE(langscore::GetScriptExtension(settings::VXAce), QString(".rb"));
}

void TestUtility::testGetScriptExtensionMV()
{
    QCOMPARE(langscore::GetScriptExtension(settings::MV), QString(".js"));
}

void TestUtility::testGetScriptExtensionMZ()
{
    QCOMPARE(langscore::GetScriptExtension(settings::MZ), QString(".js"));
}

QTEST_MAIN(TestUtility)
#include "tst_Utility.moc"
