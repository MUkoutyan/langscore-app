#pragma once

#include <QObject>
#include <QQueue>
#include <QTimer>
#include <memory>
#include "TranslationService.h"

class TranslationManager : public QObject
{
    Q_OBJECT

public:
    struct BatchTranslationRequest {
        QString text;
        QString sourceLang;
        QString targetLang;
        int row;
        int column;
        int batchId;
    };

    struct BatchTranslationResult {
        QString translatedText;
        QString originalText;
        int row;
        int column;
        int batchId;
        bool success;
        QString errorMessage;
    };

    explicit TranslationManager(QObject *parent = nullptr);
    ~TranslationManager();

    void addTranslationService(std::unique_ptr<TranslationService> service);
    void translateBatch(const QList<BatchTranslationRequest>& requests, 
                       TranslationService::ServiceType preferredService = TranslationService::ServiceType::DeepL);
    
    bool hasConfiguredService() const;
    QStringList getAvailableServices() const;

signals:
    void batchTranslationStarted(int batchId, int totalRequests);
    void translationProgress(int batchId, int completed, int total);
    void batchTranslationCompleted(int batchId, const QList<BatchTranslationResult>& results);
    void batchTranslationError(int batchId, const QString& errorMessage);

private slots:
    void onTranslationCompleted(const TranslationService::TranslationResult& result);
    void onTranslationError(int requestId, const QString& errorMessage);
    void processNextRequest();

private:
    TranslationService* getPreferredService(TranslationService::ServiceType type);
    int generateBatchId();
    
    std::vector<std::unique_ptr<TranslationService>> services;
    QQueue<BatchTranslationRequest> requestQueue;
    QMap<int, BatchTranslationRequest> pendingRequests; // requestId -> request
    QMap<int, QList<BatchTranslationResult>> batchResults; // batchId -> results
    QMap<int, int> batchRequestCounts; // batchId -> total count
    QTimer* requestTimer;
    int currentBatchId;
    int nextRequestId;
    bool processing;
    
    static const int REQUEST_DELAY_MS = 100; // Delay between requests to avoid rate limiting
};