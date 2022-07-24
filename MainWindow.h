#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QUndoView>
#include "src/ui/ComponentBase.h"
#include "src/settings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FormTaskBar;
class MainComponent;
class MainWindow : public QMainWindow, public ComponentBase
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    //reference : https://stackoverflow.com/questions/5752408/qt-resize-borderless-widget/37507341#37507341
    enum Edges {
        None        = 0x0,
        Left        = 0x1,
        Top         = 0x2,
        Right       = 0x4,
        Bottom      = 0x8,
        TopLeft     = 0x10,
        TopRight    = 0x20,
        BottomLeft  = 0x40,
        BottomRight = 0x80,
    };

    bool event(QEvent *e) override;
    void mouseHover(QHoverEvent*);
    void mouseLeave(QEvent*);
    void mousePress(QMouseEvent*);
    void mouseRealese(QMouseEvent*);
    void mouseMove(QMouseEvent*);
    void updateCursorShape(const QPoint &);
    void calculateCursorPosition(const QPoint &, Edges &);

private:
    Ui::MainWindow *ui;
    FormTaskBar* taskBar;
    MainComponent* mainComponent;
    QUndoView* undoView;

    Edges mousePressEdge;
    Edges mouseMoveEdge;

    QPoint mousePressPos;

    bool leftButtonPressed;
    bool isDragging;
    bool cursorChanged;
    QPoint draggingStartPos;
    int borderWidth;


private slots:
    void createUndoView();
    bool changeMaximumState();
    QString openOutputProjectDir(QString root);
    void openGameProject(QString path);
};
#endif // MAINWINDOW_H
