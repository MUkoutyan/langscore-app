#pragma once

#include <QWidget>
#include "../settings.h"

namespace Ui {
class MainComponent;
class WriteModeComponent;
}

class WriteModeComponent;
class MainComponent : public QWidget
{
    Q_OBJECT

public:
    explicit MainComponent(QWidget *parent = nullptr);
    ~MainComponent();

signals:
    QString requestOpenOutputDir(QString root);

protected:
    void paintEvent(QPaintEvent* p) override;

    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    bool event(QEvent* event) override;

private:
    friend WriteModeComponent;
    Ui::MainComponent *ui;
    WriteModeComponent* writeUi;

    settings setting;

    void openFiles(QString path);
    QString findGameProject(QList<QUrl> urlList);
    void toAnalyzeMode();
    void toWriteMode();

private slots:
    void invokeAnalyze();
};

