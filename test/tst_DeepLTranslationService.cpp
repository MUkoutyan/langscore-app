#include "test_common.h"
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

    QString BodyValue(const QString& key) const {
        return QUrlQuery(QString::fromUtf8(capture->lastBody)).queryItemValue(key, QUrl::FullyDecoded);
    }
};

class DeepLTranslationServiceTest : public ::testing::Test
{
protected:
    // API キーは実行ファイルと同じフォルダの settings.ini に保存されるため、前後で消しておく
    static void SetUpTestSuite() { QFile::remove(SettingsPath()); }
    static void TearDownTestSuite() { QFile::remove(SettingsPath()); }

    static QString SettingsPath() { return QCoreApplication::applicationDirPath() + "/settings.ini"; }

    static void SetApiKey(const QString& key)
    {
        QSettings settings(SettingsPath(), QSettings::IniFormat);
        settings.setValue("translation/deeplApiKey", key);
    }

    static void Translate(TestableDeepLService& service, const QString& text, const QString& targetLang)
    {
        service.translateText({text, "", targetLang, 1});
    }

    // translationError が発行された回数を数える
    static void CountErrors(TestableDeepLService& service, int& errorCount)
    {
        QObject::connect(&service, &TranslationService::translationError, [&errorCount](int, const QString&) {
            ++errorCount;
        });
    }
};

TEST_F(DeepLTranslationServiceTest, FreeKeyUsesFreeEndpoint)
{
    SetApiKey("dummy-key:fx");
    TestableDeepLService service;
    Translate(service, "Hello", "JA");
    EXPECT_EQ(service.capture->lastUrl.host(), QString("api-free.deepl.com"));
}

// 不具合の再発防止
TEST_F(DeepLTranslationServiceTest, PlusSignIsEncoded)
{
    // form-urlencoded では "+" は空白を意味するため、"%2B" にエンコードされていること
    SetApiKey("dummy-key:fx");
    TestableDeepLService service;
    Translate(service, "HP+10", "JA");
    EXPECT_TRUE(service.capture->lastBody.contains("HP%2B10")) << service.capture->lastBody.constData();
}

TEST_F(DeepLTranslationServiceTest, ProKeyUsesProEndpoint)
{
    // ":fx" で終わらない Pro 版のキーは api.deepl.com を使うこと
    SetApiKey("dummy-pro-key");
    TestableDeepLService service;
    Translate(service, "Hello", "JA");
    EXPECT_EQ(service.capture->lastUrl.host(), QString("api.deepl.com"));
}

TEST_F(DeepLTranslationServiceTest, NorwegianTarget)
{
    // CSVEditor から渡される DeepL コード "NB" が受け付けられること
    SetApiKey("dummy-key:fx");
    TestableDeepLService service;
    int errorCount = 0;
    CountErrors(service, errorCount);
    Translate(service, "Hello", TranslationService::GetTranslationLanguageCode(TranslationService::ServiceType::DeepL, "nb"));
    EXPECT_EQ(errorCount, 0);
    EXPECT_EQ(service.BodyValue("target_lang"), QString("NB"));
}

TEST_F(DeepLTranslationServiceTest, TraditionalChineseTarget)
{
    // zh-tw 列の翻訳が繁体字（ZH-HANT）として送信されること
    SetApiKey("dummy-key:fx");
    TestableDeepLService service;
    int errorCount = 0;
    CountErrors(service, errorCount);
    Translate(service, "Hello", TranslationService::GetTranslationLanguageCode(TranslationService::ServiceType::DeepL, "zh-tw"));
    EXPECT_EQ(errorCount, 0);
    EXPECT_EQ(service.BodyValue("target_lang"), QString("ZH-HANT"));
}

TEST(DeepLRetryTest, RetryOnlyWhenBusy)
{
    // 混み合っている（429）・一時的な不具合（5xx）のときだけ、待ち時間を延ばしながら送り直すこと
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(429, 0), 1000);
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(503, 2), 4000);
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(429, 0, "3"), 3000);
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(429, 5), -1);
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(200, 0), -1);
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(456, 0), -1);   //文字数の上限
    EXPECT_EQ(DeepLTranslationService::retryDelayMs(403, 0), -1);   //キーが違う
}
