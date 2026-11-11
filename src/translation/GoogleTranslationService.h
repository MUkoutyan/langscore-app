#pragma once

#include "TranslationService.h"

class GoogleTranslationService : public TranslationService
{
    Q_OBJECT

public:
    explicit GoogleTranslationService(QObject *parent = nullptr);

    ServiceType getServiceType() const override { return ServiceType::GoogleTranslate; }
    QString getServiceName() const override { return "Google Translate"; }
    bool isConfigured() const override;
    void translateText(const TranslationRequest& request) override;
    QStringList getSupportedLanguages() const override;

private slots:
    void onTranslationFinished();

private:
    QString getApiKey() const;
    void updateUsage(int charactersUsed);

    QMap<QNetworkReply*, TranslationRequest> pendingRequests;
    //requestId ごとの再送回数
    QHash<int, int> retryCounts;
};
