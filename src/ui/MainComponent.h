#pragma once

#include <QDialog>
#include "ComponentBase.h"

namespace Ui {
class AnalyzeDialog;
}

class MainComponent : public QDialog, public ComponentBase
{
    Q_OBJECT

public:
    explicit MainComponent(ComponentBase* setting, QWidget *parent = nullptr);
    ~MainComponent() override;

signals:
    QString requestOpenOutputDir(QString root);
    void toWriteMode();

public slots:
    void openGameProject(QString path);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    bool event(QEvent* event) override;

private:
    Ui::AnalyzeDialog *ui;

    void openFiles(std::pair<QString, settings::ProjectType> path);
    std::pair<QString, settings::ProjectType> findGameProject(QList<QUrl> urlList);
    void toAnalyzeMode();

private slots:
    void invokeAnalyze();


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

