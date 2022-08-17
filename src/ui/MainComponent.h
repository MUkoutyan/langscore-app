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
    explicit MainComponent(ComponentBase* setting, QWidget *parent = nullptr);
    ~MainComponent() override;

signals:
    QString requestOpenOutputDir(QString root);
    void openProject();

public slots:
    void openGameProject(QString path);

protected:
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


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

