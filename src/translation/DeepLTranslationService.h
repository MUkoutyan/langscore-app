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

    //混み合っている（429）・サーバーの一時的な不具合（5xx）のときは、待ってから送り直す。
    //送り直すまでの時間（ミリ秒）を返す。送り直さないなら -1
    static int retryDelayMs(int httpStatus, int retryCount, const QByteArray& retryAfterHeader = {});

private slots:
    void onTranslationFinished();

private:
    QString getApiKey() const;
    QString mapLanguageCode(const QString& langCode) const;
    void updateUsage(int charactersUsed);
    
    QMap<QNetworkReply*, TranslationRequest> pendingRequests;
    //requestId ごとの送り直した回数
    QHash<int, int> retryCounts;
};