#pragma once

#include <QDialog>
#include "ComponentBase.h"

namespace Ui {
class AnalyzeDialog;
}

class invoker;
class AnalyzeDialog : public QDialog, public ComponentBase
{
    Q_OBJECT

public:
    explicit AnalyzeDialog(ComponentBase* setting, QWidget *parent = nullptr);
    ~AnalyzeDialog() override;

signals:
    void toWriteMode(QString gameProjDirPath);
    void toAnalyzeMode();

public slots:
    void openFile(QString path);
    void moveParent(QPoint delta);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    Ui::AnalyzeDialog *ui;
    invoker* invoker;

private slots:
    void invokeAnalyze();


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

