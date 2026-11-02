#pragma once

#include "TranslationService.h"
#include <QSettings>
#include <QTimer>

class DeepLTranslationService : public TranslationService
{
    Q_OBJECT

public:
    explicit DeepLTranslationService(QObject *parent = nullptr);

    ServiceType getServiceType() const override { return ServiceType::DeepL; }
    QString getServiceName() const override { return "DeepL"; }
    bool isConfigured() const override;
    void translateText(const TranslationRequest& request) override;
    QStringList getSupportedLanguages() const override;

    // APIキーの種類（無料版 / Pro版）に応じたエンドポイントのURL
    static QString endpointUrl(const QString& apiKey, const QString& api = "translate");

private slots:
    void onTranslationFinished();

private:
    QString getApiKey() const;
    QString mapLanguageCode(const QString& langCode) const;
    void updateUsage(int charactersUsed);
    
    QMap<QNetworkReply*, TranslationRequest> pendingRequests;
};