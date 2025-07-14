#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class TranslationService : public QObject
{
    Q_OBJECT

public:
    enum class ServiceType {
        DeepL,
        GoogleTranslate
    };

    struct TranslationRequest {
        QString text;
        QString sourceLang;
        QString targetLang;
        int requestId;
    };

    struct TranslationResult {
        QString translatedText;
        QString originalText;
        QString sourceLang;
        QString targetLang;
        int requestId;
        bool success;
        QString errorMessage;
    };

    explicit TranslationService(QObject *parent = nullptr);
    virtual ~TranslationService() = default;

    virtual ServiceType getServiceType() const = 0;
    virtual QString getServiceName() const = 0;
    virtual bool isConfigured() const = 0;
    virtual void translateText(const TranslationRequest& request) = 0;
    virtual QStringList getSupportedLanguages() const = 0;

signals:
    void translationCompleted(const TranslationResult& result);
    void translationError(int requestId, const QString& errorMessage);

protected:
    QNetworkAccessManager* networkManager;
    static QString normalizeLanguageCode(const QString& langCode);
};