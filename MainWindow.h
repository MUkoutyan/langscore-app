#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "src/ui/FramelessWindow.h"
#include <QUndoView>
#include "src/ui/ComponentBase.h"
#include "src/settings.h"
#include "src/ui/FormTaskBar.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
class WriteModeComponent;
}
QT_END_NAMESPACE

class WriteModeComponent;
class AnalyzeDialog;
class PackingMode;
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
    PackingMode* packingUi;
    QUndoView* undoView;

    bool initialAnalysis;
    bool explicitSave;
    int lastSavedHistoryIndex;

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
