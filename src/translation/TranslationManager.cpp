#include "TranslationManager.h"
#include "DeepLTranslationService.h"
#include <QDebug>

TranslationManager::TranslationManager(QObject *parent)
    : QObject(parent)
    , requestTimer(new QTimer(this))
    , currentBatchId(1)
    , nextRequestId(1)
    , processing(false)
{
    requestTimer->setSingleShot(true);
    connect(requestTimer, &QTimer::timeout, this, &TranslationManager::processNextRequest);
    
    // Add default services
    addTranslationService(std::make_unique<DeepLTranslationService>(this));
}

TranslationManager::~TranslationManager() = default;

void TranslationManager::addTranslationService(std::unique_ptr<TranslationService> service)
{
    connect(service.get(), &TranslationService::translationCompleted,
            this, &TranslationManager::onTranslationCompleted);
    connect(service.get(), &TranslationService::translationError,
            this, &TranslationManager::onTranslationError);
    
    services.push_back(std::move(service));
}

void TranslationManager::translateBatch(const QList<BatchTranslationRequest>& requests, 
                                       TranslationService::ServiceType preferredService)
{
    if (requests.isEmpty()) return;
    
    TranslationService* service = getPreferredService(preferredService);
    if (!service) {
        emit batchTranslationError(currentBatchId, tr("No configured translation service available"));
        return;
    }
    
    int batchId = generateBatchId();
    batchRequestCounts[batchId] = requests.size();
    batchResults[batchId] = QList<BatchTranslationResult>();
    
    // Add requests to queue
    for (const auto& request : requests) {
        BatchTranslationRequest queuedRequest = request;
        queuedRequest.batchId = batchId;
        requestQueue.enqueue(queuedRequest);
    }
    
    emit batchTranslationStarted(batchId, requests.size());
    
    if (!processing) {
        processNextRequest();
    }
}

void TranslationManager::processNextRequest()
{
    if (requestQueue.isEmpty()) {
        processing = false;
        return;
    }
    
    processing = true;
    BatchTranslationRequest batchRequest = requestQueue.dequeue();
    
    TranslationService::TranslationRequest request;
    request.text = batchRequest.text;
    request.sourceLang = batchRequest.sourceLang;
    request.targetLang = batchRequest.targetLang;
    request.requestId = nextRequestId++;
    
    pendingRequests[request.requestId] = batchRequest;
    
    TranslationService* service = getPreferredService(TranslationService::ServiceType::DeepL);
    if (service) {
        service->translateText(request);
    }
    
    // Schedule next request with delay to avoid rate limiting
    if (!requestQueue.isEmpty()) {
        requestTimer->start(REQUEST_DELAY_MS);
    }
}

void TranslationManager::onTranslationCompleted(const TranslationService::TranslationResult& result)
{
    auto it = pendingRequests.find(result.requestId);
    if (it == pendingRequests.end()) return;
    
    BatchTranslationRequest batchRequest = it.value();
    pendingRequests.erase(it);
    
    BatchTranslationResult batchResult;
    batchResult.translatedText = result.translatedText;
    batchResult.originalText = result.originalText;
    batchResult.row = batchRequest.row;
    batchResult.column = batchRequest.column;
    batchResult.batchId = batchRequest.batchId;
    batchResult.success = result.success;
    batchResult.errorMessage = result.errorMessage;
    
    batchResults[batchRequest.batchId].append(batchResult);
    
    int completed = batchResults[batchRequest.batchId].size();
    int total = batchRequestCounts[batchRequest.batchId];
    
    emit translationProgress(batchRequest.batchId, completed, total);
    
    if (completed >= total) {
        emit batchTranslationCompleted(batchRequest.batchId, batchResults[batchRequest.batchId]);
        batchResults.remove(batchRequest.batchId);
        batchRequestCounts.remove(batchRequest.batchId);
    }
    
    // Continue processing if there are more requests
    if (!requestQueue.isEmpty() && !requestTimer->isActive()) {
        requestTimer->start(REQUEST_DELAY_MS);
    } else if (requestQueue.isEmpty()) {
        processing = false;
    }
}

void TranslationManager::onTranslationError(int requestId, const QString& errorMessage)
{
    auto it = pendingRequests.find(requestId);
    if (it == pendingRequests.end()) return;
    
    BatchTranslationRequest batchRequest = it.value();
    pendingRequests.erase(it);
    
    BatchTranslationResult batchResult;
    batchResult.originalText = batchRequest.text;
    batchResult.row = batchRequest.row;
    batchResult.column = batchRequest.column;
    batchResult.batchId = batchRequest.batchId;
    batchResult.success = false;
    batchResult.errorMessage = errorMessage;
    
    batchResults[batchRequest.batchId].append(batchResult);
    
    int completed = batchResults[batchRequest.batchId].size();
    int total = batchRequestCounts[batchRequest.batchId];
    
    emit translationProgress(batchRequest.batchId, completed, total);
    
    if (completed >= total) {
        emit batchTranslationCompleted(batchRequest.batchId, batchResults[batchRequest.batchId]);
        batchResults.remove(batchRequest.batchId);
        batchRequestCounts.remove(batchRequest.batchId);
    }
    
    // Continue processing
    if (!requestQueue.isEmpty() && !requestTimer->isActive()) {
        requestTimer->start(REQUEST_DELAY_MS);
    } else if (requestQueue.isEmpty()) {
        processing = false;
    }
}

TranslationService* TranslationManager::getPreferredService(TranslationService::ServiceType type)
{
    for (const auto& service : services) {
        if (service->getServiceType() == type && service->isConfigured()) {
            return service.get();
        }
    }
    
    // Fallback to any configured service
    for (const auto& service : services) {
        if (service->isConfigured()) {
            return service.get();
        }
    }
    
    return nullptr;
}

bool TranslationManager::hasConfiguredService() const
{
    for (const auto& service : services) {
        if (service->isConfigured()) {
            return true;
        }
    }
    return false;
}

QStringList TranslationManager::getAvailableServices() const
{
    QStringList serviceNames;
    for (const auto& service : services) {
        if (service->isConfigured()) {
            serviceNames.append(service->getServiceName());
        }
    }
    return serviceNames;
}

int TranslationManager::generateBatchId()
{
    return currentBatchId++;
}