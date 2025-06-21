#pragma once

#include <QObject>
#include <QMutex>
#include <QImage>
#include <QString>
#include <QList>
#include <QPointer>
#include <QTreeWidgetItem>

class GraphicsImageLoader : public QObject
{
    Q_OBJECT
public:
    explicit GraphicsImageLoader(QObject* parent = nullptr);
    ~GraphicsImageLoader();

    void requestImage(const QString& filePath, QTreeWidgetItem* item, int column, QSize resized = {});

signals:
    void imageLoaded(const QString& filePath, QTreeWidgetItem* item, int column, const QImage& image);

public slots:
    void processQueue();

private:
    struct Request {
        QString filePath;
        QTreeWidgetItem* item;
        int column;
        QSize resizedSize;
    };
    QList<Request> loadRequestQueue;
    QMutex mutex;
    bool isProcessing = false;
};