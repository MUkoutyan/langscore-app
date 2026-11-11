#include "GoogleTranslationService.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QSettings>
#include <QApplication>
#include <QTimer>

GoogleTranslationService::GoogleTranslationService(QObject *parent)
    : TranslationService(parent)
{
}

bool GoogleTranslationService::isConfigured() const
{
    return !getApiKey().isEmpty();
}

QString GoogleTranslationService::getApiKey() const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    return settings.value("translation/googleApiKey", "").toString();
}

void GoogleTranslationService::translateText(const TranslationRequest& request)
{
    if (!isConfigured()) {
        emit translationError(request.requestId, tr("Google Translate API key not configured"));
        return;
    }

    //言語コードは CSVEditor で GetTranslationLanguageCode により Google 用に変換済み
    if (request.targetLang.isEmpty()) {
        emit translationError(request.requestId, tr("Unsupported target language: %1").arg(request.targetLang));
        return;
    }

    QUrl url("https://translation.googleapis.com/language/translate/v2");
    QUrlQuery query;
    query.addQueryItem("key", getApiKey());
    url.setQuery(query);

    QNetworkRequest networkRequest(url);
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["q"] = request.text;
    body["target"] = request.targetLang;
    //"html" のままだと記号が "&#39;" のような文字参照で返されるため、"text" を指定する
    body["format"] = "text";
    if (!request.sourceLang.isEmpty()) {
        body["source"] = request.sourceLang;
    }

    QNetworkReply* reply = networkManager->post(networkRequest, QJsonDocument(body).toJson(QJsonDocument::Compact));
    pendingRequests[reply] = request;

    connect(reply, &QNetworkReply::finished, this, &GoogleTranslationService::onTranslationFinished);

    QTimer::singleShot(30000, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
}

void GoogleTranslationService::onTranslationFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    reply->deleteLater();

    auto it = pendingRequests.find(reply);
    if (it == pendingRequests.end()) return;

    TranslationRequest request = it.value();
    pendingRequests.erase(it);

    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const int retryCount = retryCounts.value(request.requestId, 0);
    const int delay = retryDelayMs(httpStatus, retryCount, reply->rawHeader("Retry-After"));
    if(0 <= delay) {
        retryCounts[request.requestId] = retryCount + 1;
        QTimer::singleShot(delay, this, [this, request]() { this->translateText(request); });
        return;
    }
    retryCounts.remove(request.requestId);

    const QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();

    if (reply->error() == QNetworkReply::NoError) {
        const QJsonArray translations = obj["data"].toObject()["translations"].toArray();
        if (!translations.isEmpty()) {
            TranslationResult result;
            result.originalText = request.text;
            result.sourceLang = request.sourceLang;
            result.targetLang = request.targetLang;
            result.requestId = request.requestId;
            result.translatedText = translations[0].toObject()["translatedText"].toString();
            result.success = true;

            updateUsage(request.text.length());

            emit translationCompleted(result);
            return;
        }
        emit translationError(request.requestId, tr("Invalid response from Google Translate API"));
        return;
    }

    //キーの誤りなどの理由は、レスポンスの error.message に書かれている
    const QString apiMessage = obj["error"].toObject()["message"].toString();
    emit translationError(request.requestId, apiMessage.isEmpty() ? reply->errorString() : apiMessage);
}

QStringList GoogleTranslationService::getSupportedLanguages() const
{
    return {
        "en", "ja", "zh", "zh-CN", "zh-TW", "de", "fr", "es", "it", "pt", "ru", "ko",
        "nl", "pl", "sv", "da", "no", "fi", "cs", "hu", "ro", "sk",
        "sl", "bg", "et", "lv", "lt", "uk", "tr", "el", "ar", "hi",
        "id", "ms", "th", "vi"
    };
}

void GoogleTranslationService::updateUsage(int charactersUsed)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    qint64 currentUsage = settings.value("translation/googleUsage", 0).toLongLong();
    settings.setValue("translation/googleUsage", currentUsage + charactersUsed);
}
