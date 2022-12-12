#pragma once

#include <QMainWindow>

class FramelessWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit FramelessWindow(QWidget *parent = nullptr);

public slots:
    bool changeMaximumState();

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

    void updateCursorShape(const QPoint &);
    void calculateCursorPosition(const QPoint &, Edges &);

    bool event(QEvent *e) override;
    void mouseHover(QHoverEvent*);
    void mouseLeave(QEvent*);
    void mousePress(QMouseEvent*);
    void mouseRealese(QMouseEvent*);
    void mouseMove(QMouseEvent*);


    Edges mousePressEdge;
    Edges mouseMoveEdge;

    QPoint mousePressPos;

    bool leftButtonPressed;
    bool isDragging;
    bool cursorChanged;
    QPoint draggingStartPos;
    int borderWidth;


};

