#include <QTest>
#include "LanguageNames.h"

class TestLanguageNames : public QObject
{
    Q_OBJECT

private slots:
    void testKnownLanguages_data();
    void testKnownLanguages();
    void testEmptyInput();
    void testUnknownInput();
    void testUnderscoreNormalization();
    void testCaseInsensitive();
    void testBcp47Fallback();
    void testMapNotEmpty();
};

void TestLanguageNames::testKnownLanguages_data()
{
    QTest::addColumn<QString>("code");

    QTest::newRow("en")    << "en";
    QTest::newRow("ja")    << "ja";
    QTest::newRow("de")    << "de";
    QTest::newRow("fr")    << "fr";
    QTest::newRow("zh")    << "zh";
    QTest::newRow("ko")    << "ko";
    QTest::newRow("zh-cn") << "zh-cn";
    QTest::newRow("zh-tw") << "zh-tw";
}

void TestLanguageNames::testKnownLanguages()
{
    QFETCH(QString, code);
    QVERIFY(langscore::languageDisplayName(code).isEmpty() == false);
}

void TestLanguageNames::testEmptyInput()
{
    QCOMPARE(langscore::languageDisplayName(""), QString());
}

void TestLanguageNames::testUnknownInput()
{
    QCOMPARE(langscore::languageDisplayName("xx-zz"), QString());
}

void TestLanguageNames::testUnderscoreNormalization()
{
    // "_" は "-" に正規化されるため zh_CN → zh-cn として検索される
    QVERIFY(langscore::languageDisplayName("zh_CN").isEmpty() == false);
    QVERIFY(langscore::languageDisplayName("zh_TW").isEmpty() == false);
}

void TestLanguageNames::testCaseInsensitive()
{
    // 大文字小文字を区別しない（toLower で正規化される）
    QVERIFY(langscore::languageDisplayName("EN").isEmpty() == false);
    QVERIFY(langscore::languageDisplayName("JA").isEmpty() == false);
}

void TestLanguageNames::testBcp47Fallback()
{
    // en-US → en にフォールバックして一致
    QVERIFY(langscore::languageDisplayName("en-US").isEmpty() == false);
    // zh-Hant-HK → zh-hant-hk → zh-hant → zh でフォールバック
    QVERIFY(langscore::languageDisplayName("zh-Hant-HK").isEmpty() == false);
}

void TestLanguageNames::testMapNotEmpty()
{
    const auto& map = langscore::languageDisplayMap();
    QVERIFY(map.empty() == false);
}

QTEST_MAIN(TestLanguageNames)
#include "tst_LanguageNames.moc"
