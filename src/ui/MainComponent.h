#pragma once

#include <QWidget>
#include "ComponentBase.h"

namespace Ui {
class MainComponent;
class WriteModeComponent;
}

class WriteModeComponent;
class MainComponent : public QWidget, public ComponentBase
{
    Q_OBJECT

public:
    explicit MainComponent(Common::Type setting, QWidget *parent = nullptr);
    ~MainComponent() override;

signals:
    QString requestOpenOutputDir(QString root);

public slots:
    void openGameProject(QString path);

protected:
    void paintEvent(QPaintEvent* p) override;

    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    bool event(QEvent* event) override;

private:
    friend WriteModeComponent;
    Ui::MainComponent *ui;
    WriteModeComponent* writeUi;

    void openFiles(std::pair<QString, settings::ProjectType> path);
    std::pair<QString, settings::ProjectType> findGameProject(QList<QUrl> urlList);
    void toAnalyzeMode();
    void toWriteMode();

private slots:
    void invokeAnalyze();
};

