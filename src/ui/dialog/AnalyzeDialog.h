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
    //翻訳作業を開始し、ゲームにプラグインと翻訳CSVを書き出したとき
    void translationStarted();

public slots:
    void openFile(QString path);
    void moveParent(QPoint delta);

protected:
    void dropEvent(QDropEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

private:
    //翻訳作業の開始で、divisi を順に実行する。解析→プラグインと翻訳CSVの書き出し→パッキング（VXAceのみ）
    enum class InvokeStep {
        None,
        Analyze,
        Export,
        Packing
    };

    Ui::AnalyzeDialog *ui;
    invoker* invoker;
    InvokeStep invokeStep = InvokeStep::None;

    void exportToGame();
    void finishStartTranslation();

private slots:
    void invokeAnalyze();


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

