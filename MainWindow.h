#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "src/ui/FramelessWindow.h"
#include <QUndoView>
#include "src/ui/ComponentBase.h"
#include "src/settings.h"
#include "src/ui/FormTaskBar.h"
#include "src/service/ValidationService.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
class WriteModeComponent;
}
QT_END_NAMESPACE

class WriteModeComponent;
class AnalyzeDialog;
class invoker;
class MainWindow : public FramelessWindow, public ComponentBase
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void closeEvent(QCloseEvent*) override;

private:
    Ui::MainWindow *ui;
    FormTaskBar* taskBar;
    AnalyzeDialog* analyzeDialog;
    WriteModeComponent* writeUi;
    langscore::ValidationService* validationService;
    QUndoView* undoView;

    bool _finishInvoke;

    bool initialAnalysis;
    // 初回解析を行ったプロジェクトの langscore フォルダ。破棄時に削除してよいのはこのフォルダのみ
    QString initialAnalysisProjectDir;
    bool explicitSave;

    void receive(DispatchType type, const QVariantList& args) override;
    int askCloseProject();

    void attachTheme(ColorTheme::Theme theme);

signals:
    void moveWiget(QPoint newPos);

private slots:
    void createUndoView();
    void openGameProject(QString configFilePath);
    void showWriteMode();



#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};
#endif // MAINWINDOW_H
