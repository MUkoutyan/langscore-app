#include "DeepLTranslationService.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrlQuery>
#include <QApplication>
#include <QTimer>

DeepLTranslationService::DeepLTranslationService(QObject *parent)
    : TranslationService(parent)
{
}

bool DeepLTranslationService::isConfigured() const
{
    return !getApiKey().isEmpty();
}

QString DeepLTranslationService::getApiKey() const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    return settings.value("translation/deeplApiKey", "").toString();
}

void DeepLTranslationService::translateText(const TranslationRequest& request)
{
    if (!isConfigured()) {
        emit translationError(request.requestId, tr("DeepL API key not configured"));
        return;
    }

    QString apiKey = getApiKey();
    QString targetLang = mapLanguageCode(request.targetLang);
    
    if (targetLang.isEmpty()) {
        emit translationError(request.requestId, tr("Unsupported target language: %1").arg(request.targetLang));
        return;
    }

    QUrl url(endpointUrl(apiKey));
    QNetworkRequest networkRequest(url);
    networkRequest.setRawHeader("Authorization", QString("DeepL-Auth-Key %1").arg(apiKey).toUtf8());
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("text", request.text);
    postData.addQueryItem("target_lang", targetLang);
    
    // Add source language if specified
    if (!request.sourceLang.isEmpty()) {
        //source_lang は "ZH-HANT" のような地域・字体付きのコードを受け付けない
        QString sourceLang = mapLanguageCode(request.sourceLang).section('-', 0, 0);
        if (!sourceLang.isEmpty()) {
            postData.addQueryItem("source_lang", sourceLang);
        }
    }

    //QUrlQuery は "+" をエンコードしないが、form-urlencoded では "+" が空白を意味するため明示的にエンコードする
    auto body = postData.toString(QUrl::FullyEncoded).replace("+", "%2B").toUtf8();
    QNetworkReply* reply = networkManager->post(networkRequest, body);
    pendingRequests[reply] = request;
    
    connect(reply, &QNetworkReply::finished, this, &DeepLTranslationService::onTranslationFinished);
    
    // Set timeout
    QTimer::singleShot(30000, reply, [reply]() {
        if (reply->isRunning()) {
            reply->abort();
        }
    });
}

void DeepLTranslationService::onTranslationFinished()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    reply->deleteLater();
    
    auto it = pendingRequests.find(reply);
    if (it == pendingRequests.end()) return;
    
    TranslationRequest request = it.value();
    pendingRequests.erase(it);

    //多くのセルを続けて翻訳すると DeepL が 429 を返すことがあるため、待ってから再送する
    const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const int retryCount = retryCounts.value(request.requestId, 0);
    const int delay = retryDelayMs(httpStatus, retryCount, reply->rawHeader("Retry-After"));
    if(0 <= delay) {
        retryCounts[request.requestId] = retryCount + 1;
        QTimer::singleShot(delay, this, [this, request]() { this->translateText(request); });
        return;
    }
    retryCounts.remove(request.requestId);
    
    TranslationResult result;
    result.originalText = request.text;
    result.sourceLang = request.sourceLang;
    result.targetLang = request.targetLang;
    result.requestId = request.requestId;
    
    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("translations")) {
                QJsonArray translations = obj["translations"].toArray();
                if (!translations.isEmpty()) {
                    QJsonObject translation = translations[0].toObject();
                    result.translatedText = translation["text"].toString();
                    result.success = true;
                    
                    // Update usage statistics
                    updateUsage(request.text.length());
                    
                    emit translationCompleted(result);
                    return;
                }
            }
        }
        result.success = false;
        result.errorMessage = tr("Invalid response from DeepL API");
    } else {
        result.success = false;
        result.errorMessage = reply->errorString();
    }
    
    emit translationError(result.requestId, result.errorMessage);
}

QString DeepLTranslationService::endpointUrl(const QString& apiKey, const QString& api)
{
    //無料版のキーは ":fx" で終わる。それ以外は Pro 版のエンドポイントを使う
    const QString host = apiKey.endsWith(":fx") ? "api-free.deepl.com" : "api.deepl.com";
    return QString("https://%1/v2/%2").arg(host, api);
}

int DeepLTranslationService::retryDelayMs(int httpStatus, int retryCount, const QByteArray& retryAfterHeader)
{
    constexpr int MaxRetry = 5;
    const bool retryable = httpStatus == 429 || (500 <= httpStatus && httpStatus < 600);
    if(retryable == false || MaxRetry <= retryCount) { return -1; }
    bool isOk = false;
    const int retryAfterSec = retryAfterHeader.toInt(&isOk);
    if(isOk && 0 < retryAfterSec) { return std::min(retryAfterSec, 60) * 1000; }
    return 1000 << retryCount;  //1, 2, 4, 8, 16 秒
}

QString DeepLTranslationService::mapLanguageCode(const QString& langCode) const
{
    //"NB", "ZH-HANT" など、GetTranslationLanguageCode で変換済みの DeepL コードはそのまま使う
    const QString upper = langCode.trimmed().toUpper();
    if(getSupportedLanguages().contains(upper)) {
        return upper;
    }

    QString normalized = normalizeLanguageCode(langCode).toLower();

    // DeepL supported languages mapping
    static const QMap<QString, QString> deeplLangCodes = {
        {"en", "EN"},
        {"ja", "JA"},
        {"zh", "ZH"},
        {"zh-tw", "ZH-HANT"},
        {"de", "DE"},
        {"fr", "FR"},
        {"es", "ES"},
        {"it", "IT"},
        {"pt", "PT"},
        {"ru", "RU"},
        {"ko", "KO"},
        {"nl", "NL"},
        {"pl", "PL"},
        {"sv", "SV"},
        {"da", "DA"},
        {"no", "NB"},
        {"fi", "FI"},
        {"cs", "CS"},
        {"hu", "HU"},
        {"ro", "RO"},
        {"sk", "SK"},
        {"sl", "SL"},
        {"bg", "BG"},
        {"et", "ET"},
        {"lv", "LV"},
        {"lt", "LT"},
        {"uk", "UK"},
        {"tr", "TR"},
        {"el", "EL"},
        {"ar", "AR"},
        {"hi", "HI"},
        {"id", "ID"},
        {"ms", "MS"},
        {"th", "TH"},
        {"vi", "VI"}
    };
    
    return deeplLangCodes.value(normalized, "");
}

QStringList DeepLTranslationService::getSupportedLanguages() const
{
    return {
        "EN", "JA", "ZH", "ZH-HANS", "ZH-HANT", "DE", "FR", "ES", "IT", "PT", "RU", "KO",
        "NL", "PL", "SV", "DA", "NB", "FI", "CS", "HU", "RO", "SK",
        "SL", "BG", "ET", "LV", "LT", "UK", "TR", "EL", "AR", "HI",
        "ID", "MS", "TH", "VI"
    };
}

void DeepLTranslationService::updateUsage(int charactersUsed)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    qint64 currentUsage = settings.value("translation/deeplUsage", 0).toLongLong();
    settings.setValue("translation/deeplUsage", currentUsage + charactersUsed);
}