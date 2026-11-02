#include <QTest>
#include "translation/TranslationService.h"

// normalizeLanguageCode は protected なのでサブクラス経由で呼び出す
struct DummyService : public TranslationService
{
    using TranslationService::normalizeLanguageCode;
    ServiceType getServiceType() const override { return ServiceType::DeepL; }
    QString getServiceName() const override { return {}; }
    bool isConfigured() const override { return false; }
    void translateText(const TranslationRequest&) override {}
    QStringList getSupportedLanguages() const override { return {}; }
};

using ServiceType = TranslationService::ServiceType;

class TestTranslationService : public QObject
{
    Q_OBJECT

private slots:
    void testLanguageCode_data();
    void testLanguageCode();
    void testLanguageCodeUnknown();
    void testNormalize_data();
    void testNormalize();

    // 不具合の再発防止
    void testDeepLTraditionalChinese();
};

void TestTranslationService::testLanguageCode_data()
{
    QTest::addColumn<int>("service");
    QTest::addColumn<QString>("name");
    QTest::addColumn<QString>("expected");

    QTest::newRow("deepl en")       << int(ServiceType::DeepL) << "en" << "EN";
    QTest::newRow("deepl japanese") << int(ServiceType::DeepL) << "japanese" << "JA";
    QTest::newRow("deepl zh-tw")    << int(ServiceType::DeepL) << "zh-tw" << "ZH-HANT";
    QTest::newRow("google ja")      << int(ServiceType::GoogleTranslate) << "ja" << "ja";
    QTest::newRow("google zh-cn")   << int(ServiceType::GoogleTranslate) << "zh-cn" << "zh-CN";
    QTest::newRow("google zh-tw")   << int(ServiceType::GoogleTranslate) << "zh-tw" << "zh-TW";
}

void TestTranslationService::testLanguageCode()
{
    QFETCH(int, service);
    QFETCH(QString, name);
    QFETCH(QString, expected);
    QCOMPARE(TranslationService::GetTranslationLanguageCode(ServiceType(service), name), expected);
}

void TestTranslationService::testLanguageCodeUnknown()
{
    QCOMPARE(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, "klingon"), QString());
    QCOMPARE(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, ""), QString());
}

void TestTranslationService::testNormalize_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("lower")       << "EN" << "en";
    QTest::newRow("trim")        << "  ja  " << "ja";
    QTest::newRow("alias")       << "Japanese" << "ja";
    QTest::newRow("prefix")      << "lang_jp" << "ja";
    QTest::newRow("suffix")      << "en_lang" << "en";
    QTest::newRow("zh_TW")       << "zh_TW" << "zh-TW";
    QTest::newRow("passthrough") << "xx" << "xx";
}

void TestTranslationService::testNormalize()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);
    QCOMPARE(DummyService::normalizeLanguageCode(input), expected);
}

void TestTranslationService::testDeepLTraditionalChinese()
{
    // 繁体字は DeepL の繁体字ターゲット（ZH-HANT）に変換されること。"ZH" は簡体字になる
    QCOMPARE(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, "zh-tw"), QString("ZH-HANT"));
    QCOMPARE(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, "traditional chinese"), QString("ZH-HANT"));
}

QTEST_MAIN(TestTranslationService)
#include "tst_TranslationService.moc"
