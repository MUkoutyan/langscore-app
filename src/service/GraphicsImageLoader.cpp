#include "GraphicsImageLoader.h"
#include <QThread>
#include <QImage>
#include <QMutexLocker>

GraphicsImageLoader::GraphicsImageLoader(QObject* parent)
    : QObject(parent)
{
}

GraphicsImageLoader::~GraphicsImageLoader()
{
}

void GraphicsImageLoader::requestImage(const QString& filePath, QTreeWidgetItem* item, int column, QSize resized)
{
    QMutexLocker locker(&mutex);
    loadRequestQueue.append({filePath, item, column, resized});
    if (isProcessing == false) {
        isProcessing = true;
        QMetaObject::invokeMethod(this, "processQueue", Qt::QueuedConnection);
    }
}

void GraphicsImageLoader::processQueue()
{
    Request req;
    {
        QMutexLocker locker(&mutex);
        if (loadRequestQueue.isEmpty()) {
            isProcessing = false;
            return;
        }
        req = loadRequestQueue.takeFirst();
    }
    if (req.item) {
        if(req.resizedSize.isNull()) {
            QImage img(req.filePath);
            emit imageLoaded(req.filePath, req.item, req.column, img);
        }
        else {
            QImage img(req.filePath);
            img = img.scaled(req.resizedSize, Qt::KeepAspectRatio, Qt::FastTransformation);
            emit imageLoaded(req.filePath, req.item, req.column, img);
        }
    }
    // Continue processing
    QMetaObject::invokeMethod(this, "processQueue", Qt::QueuedConnection);
}