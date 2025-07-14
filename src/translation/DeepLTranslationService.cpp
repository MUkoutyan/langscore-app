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

    // Use free API endpoint
    QUrl url("https://api-free.deepl.com/v2/translate");
    QNetworkRequest networkRequest(url);
    networkRequest.setRawHeader("Authorization", QString("DeepL-Auth-Key %1").arg(apiKey).toUtf8());
    networkRequest.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");

    QUrlQuery postData;
    postData.addQueryItem("text", request.text);
    postData.addQueryItem("target_lang", targetLang);
    
    // Add source language if specified
    if (!request.sourceLang.isEmpty()) {
        QString sourceLang = mapLanguageCode(request.sourceLang);
        if (!sourceLang.isEmpty()) {
            postData.addQueryItem("source_lang", sourceLang);
        }
    }

    QNetworkReply* reply = networkManager->post(networkRequest, postData.toString(QUrl::FullyEncoded).toUtf8());
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

QString DeepLTranslationService::mapLanguageCode(const QString& langCode) const
{
    QString normalized = normalizeLanguageCode(langCode);
    
    // DeepL supported languages mapping
    static const QMap<QString, QString> deeplLangCodes = {
        {"en", "EN"},
        {"ja", "JA"},
        {"zh", "ZH"},
        {"zh-tw", "ZH"},
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
        "EN", "JA", "ZH", "DE", "FR", "ES", "IT", "PT", "RU", "KO",
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