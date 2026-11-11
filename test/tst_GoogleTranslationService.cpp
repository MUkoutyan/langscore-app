#include "test_common.h"
#include <QSettings>
#include <QFile>
#include <QNetworkAccessManager>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include "translation/GoogleTranslationService.h"

namespace
{
// 送信内容を記録し、実際の通信は行わない NetworkAccessManager
class GoogleCapturingNetworkAccessManager : public QNetworkAccessManager
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

class TestableGoogleService : public GoogleTranslationService
{
public:
    GoogleCapturingNetworkAccessManager* capture = new GoogleCapturingNetworkAccessManager(this);
    TestableGoogleService() { networkManager = capture; }

    QString BodyValue(const QString& key) const {
        return QJsonDocument::fromJson(capture->lastBody).object()[key].toString();
    }
};
}

class GoogleTranslationServiceTest : public ::testing::Test
{
protected:
    // API キーは実行ファイルと同じフォルダの settings.ini に保存されるため、前後で削除する
    static void SetUpTestSuite() { QFile::remove(SettingsPath()); }
    static void TearDownTestSuite() { QFile::remove(SettingsPath()); }

    static QString SettingsPath() { return QCoreApplication::applicationDirPath() + "/settings.ini"; }

    static void SetApiKey(const QString& key)
    {
        QSettings settings(SettingsPath(), QSettings::IniFormat);
        settings.setValue("translation/googleApiKey", key);
    }
};

TEST_F(GoogleTranslationServiceTest, NotConfiguredWithoutKey)
{
    SetApiKey("");
    TestableGoogleService service;
    EXPECT_FALSE(service.isConfigured());
}

TEST_F(GoogleTranslationServiceTest, RequestContent)
{
    // API キーは URL の key に、本文は JSON で送信されること
    SetApiKey("dummy-google-key");
    TestableGoogleService service;
    const auto lang = [](const QString& name) {
        return TranslationService::GetTranslationLanguageCode(TranslationService::ServiceType::GoogleTranslate, name);
    };
    service.translateText({"HP+10 & 'MP'", lang("ja"), lang("zh-tw"), 1});

    EXPECT_EQ(service.capture->lastUrl.host(), QString("translation.googleapis.com"));
    EXPECT_EQ(QUrlQuery(service.capture->lastUrl).queryItemValue("key"), QString("dummy-google-key"));
    EXPECT_EQ(service.BodyValue("q"), QString("HP+10 & 'MP'"));
    EXPECT_EQ(service.BodyValue("source"), QString("ja"));
    EXPECT_EQ(service.BodyValue("target"), QString("zh-TW"));
    // "html" だと記号が文字参照で返されるため、"text" であること
    EXPECT_EQ(service.BodyValue("format"), QString("text"));
}
