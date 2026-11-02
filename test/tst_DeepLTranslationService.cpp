#include <QTest>
#include <QSignalSpy>
#include <QSettings>
#include <QFile>
#include <QNetworkAccessManager>
#include <QUrlQuery>
#include "translation/DeepLTranslationService.h"

// 送信内容を記録し、実際の通信は行わない NetworkAccessManager
class CapturingNetworkAccessManager : public QNetworkAccessManager
{
public:
    using QNetworkAccessManager::QNetworkAccessManager;
    QUrl lastUrl;
    QByteArray lastBody;

protected:
    QNetworkReply* createRequest(Operation op, const QNetworkRequest& request, QIODevice* outgoingData) override
    {
        lastUrl = request.url();
        lastBody = outgoingData ? outgoingData->readAll() : QByteArray();
        return QNetworkAccessManager::createRequest(op, QNetworkRequest(QUrl("unsupported://none")), nullptr);
    }
};

class TestableDeepLService : public DeepLTranslationService
{
public:
    CapturingNetworkAccessManager* capture = new CapturingNetworkAccessManager(this);
    TestableDeepLService() { networkManager = capture; }

    QString bodyValue(const QString& key) const {
        return QUrlQuery(QString::fromUtf8(capture->lastBody)).queryItemValue(key, QUrl::FullyDecoded);
    }
};

class TestDeepLTranslationService : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void testFreeKeyUsesFreeEndpoint();

    // 不具合の再発防止
    void testPlusSignIsEncoded();
    void testProKeyUsesProEndpoint();
    void testNorwegianTarget();
    void testTraditionalChineseTarget();

private:
    QString settingsPath() const { return QCoreApplication::applicationDirPath() + "/settings.ini"; }
    void setApiKey(const QString& key);
    void translate(TestableDeepLService& service, const QString& text, const QString& targetLang);
};

void TestDeepLTranslationService::initTestCase()
{
    QFile::remove(settingsPath());
}

void TestDeepLTranslationService::cleanupTestCase()
{
    QFile::remove(settingsPath());
}

void TestDeepLTranslationService::setApiKey(const QString& key)
{
    QSettings settings(settingsPath(), QSettings::IniFormat);
    settings.setValue("translation/deeplApiKey", key);
}

void TestDeepLTranslationService::translate(TestableDeepLService& service, const QString& text, const QString& targetLang)
{
    service.translateText({text, "", targetLang, 1});
}

void TestDeepLTranslationService::testFreeKeyUsesFreeEndpoint()
{
    setApiKey("dummy-key:fx");
    TestableDeepLService service;
    translate(service, "Hello", "JA");
    QCOMPARE(service.capture->lastUrl.host(), QString("api-free.deepl.com"));
}

void TestDeepLTranslationService::testPlusSignIsEncoded()
{
    // form-urlencoded では "+" は空白を意味するため、"%2B" にエンコードされていること
    setApiKey("dummy-key:fx");
    TestableDeepLService service;
    translate(service, "HP+10", "JA");
    QVERIFY2(service.capture->lastBody.contains("HP%2B10"), service.capture->lastBody.constData());
}

void TestDeepLTranslationService::testProKeyUsesProEndpoint()
{
    // ":fx" で終わらない Pro 版のキーは api.deepl.com を使うこと
    setApiKey("dummy-pro-key");
    TestableDeepLService service;
    translate(service, "Hello", "JA");
    QCOMPARE(service.capture->lastUrl.host(), QString("api.deepl.com"));
}

void TestDeepLTranslationService::testNorwegianTarget()
{
    // CSVEditor から渡される DeepL コード "NB" が受け付けられること
    setApiKey("dummy-key:fx");
    TestableDeepLService service;
    QSignalSpy errorSpy(&service, &TranslationService::translationError);
    translate(service, "Hello", TranslationService::GetTranslationLanguageCode(TranslationService::ServiceType::DeepL, "nb"));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(service.bodyValue("target_lang"), QString("NB"));
}

void TestDeepLTranslationService::testTraditionalChineseTarget()
{
    // zh-tw 列の翻訳が繁体字（ZH-HANT）として送信されること
    setApiKey("dummy-key:fx");
    TestableDeepLService service;
    QSignalSpy errorSpy(&service, &TranslationService::translationError);
    translate(service, "Hello", TranslationService::GetTranslationLanguageCode(TranslationService::ServiceType::DeepL, "zh-tw"));
    QCOMPARE(errorSpy.count(), 0);
    QCOMPARE(service.bodyValue("target_lang"), QString("ZH-HANT"));
}

QTEST_MAIN(TestDeepLTranslationService)
#include "tst_DeepLTranslationService.moc"
