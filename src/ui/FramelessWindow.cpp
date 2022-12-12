#include "FramelessWindow.h"
#include <QEvent>
#include <QMouseEvent>

FramelessWindow::FramelessWindow(QWidget *parent)
    : QMainWindow{parent}
    , mousePressEdge(Edges::None)
    , mouseMoveEdge(Edges::None)
    , mousePressPos()
    , leftButtonPressed(false)
    , isDragging(false)
    , cursorChanged(false)
    , draggingStartPos()
    , borderWidth(2)
{
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setAttribute(Qt::WA_Hover);
    this->setAutoFillBackground(true);
}


bool FramelessWindow::changeMaximumState()
{
    if(this->isMaximized()){
        this->showNormal();
        return false;
    }
    else {
        this->showMaximized();
        return true;
    }
}

bool FramelessWindow::event(QEvent* e)
{
    switch (e->type()) {
    case QEvent::MouseMove:
        mouseMove(static_cast<QMouseEvent*>(e));
        break;
    case QEvent::HoverMove:
        mouseHover(static_cast<QHoverEvent*>(e));
        break;
    case QEvent::Leave:
        mouseLeave(e);
        break;
    case QEvent::MouseButtonPress:
        mousePress(static_cast<QMouseEvent*>(e));
        break;
    case QEvent::MouseButtonRelease:
        mouseRealese(static_cast<QMouseEvent*>(e));
        break;
    default:
        break;
    }
    return QMainWindow::event(e);
}


void FramelessWindow::mouseHover(QHoverEvent *e) {
    updateCursorShape(QCursor::pos());
}

void FramelessWindow::mouseLeave(QEvent*) {
    if (!leftButtonPressed) {
        this->unsetCursor();
    }
}

void FramelessWindow::mousePress(QMouseEvent *e) {
    if (e->button() & Qt::LeftButton) {
        leftButtonPressed = true;
        calculateCursorPosition(QCursor::pos(), mousePressEdge);

        if (this->rect().marginsRemoved(QMargins(borderWidth, borderWidth, borderWidth, borderWidth)).contains(e->pos())) {
            isDragging = true;
            draggingStartPos = e->pos();
        }
    }
}

void FramelessWindow::mouseRealese(QMouseEvent *e) {
    if (e->button() & Qt::LeftButton) {
        leftButtonPressed = false;
        isDragging = false;
        mousePressPos = QPoint();
    }
}

void FramelessWindow::mouseMove(QMouseEvent *e)
{
    if (leftButtonPressed == false) {
        updateCursorShape(QCursor::pos());
        return;
    }

    if(mousePressEdge == Edges::None){ return; }

    auto rect = this->frameGeometry();
    auto pos = QCursor::pos();
    int top = rect.top(), bottom = rect.bottom(), left = rect.left(), right = rect.right();

    switch (mousePressEdge) {
    case Edges::Top:
        top = pos.y();
        break;
    case Edges::Bottom:
        bottom = pos.y();
        break;
    case Edges::Left:
        left = pos.x();
        break;
    case Edges::Right:
        right = pos.x();
        break;
    case Edges::TopLeft:
        top = pos.y();
        left = pos.x();
        break;
    case Edges::TopRight:
        right = pos.x();
        top = pos.y();
        break;
    case Edges::BottomLeft:
        bottom = pos.y();
        left = pos.x();
        break;
    case Edges::BottomRight:
        bottom = pos.y();
        right = pos.x();
        break;
    default:
        break;
    }
    QRect newRect(QPoint(left, top), QPoint(right, bottom));
    if (newRect.width() < this->minimumWidth()) {
        newRect.setLeft(this->frameGeometry().x());
    }
    else if (newRect.height() < this->minimumHeight()) {
        newRect.setTop(this->frameGeometry().y());
    }
    this->resize(newRect.size());
    this->move(newRect.topLeft());
    this->update();
}


void FramelessWindow::updateCursorShape(const QPoint &pos)
{
    if (this->isFullScreen() || this->isMaximized()) {
        if (cursorChanged) {
            this->unsetCursor();
        }
        return;
    }
    if(leftButtonPressed){ return; }

    calculateCursorPosition(pos, mouseMoveEdge);
    cursorChanged = true;
    if (mouseMoveEdge&Edges::Top || mouseMoveEdge&Edges::Bottom) {
        this->setCursor(Qt::SizeVerCursor);
    }
    else if (mouseMoveEdge&Edges::Left || mouseMoveEdge&Edges::Right) {
        this->setCursor(Qt::SizeHorCursor);
    }
    else if (mouseMoveEdge&Edges::TopLeft || mouseMoveEdge&Edges::BottomRight) {
        this->setCursor(Qt::SizeFDiagCursor);
    }
    else if (mouseMoveEdge&Edges::TopRight || mouseMoveEdge&Edges::BottomLeft) {
        this->setCursor(Qt::SizeBDiagCursor);
    }
    else if (cursorChanged) {
        this->unsetCursor();
        cursorChanged = false;
    }
}

void FramelessWindow::calculateCursorPosition(const QPoint &pos, Edges &_edge)
{
    auto rect = this->frameGeometry();
    auto border = borderWidth*2;

    if (QRect(rect.x()-border, rect.bottom()-border, border*2, border*2).contains(pos)) {
        _edge = BottomLeft;
    }
    else if (QRect(rect.right()-border, rect.bottom()-border, border*2, border*2).contains(pos)) {
        _edge = BottomRight;
    }
    else if (QRect(rect.right()-border, rect.top(), border*2, border).contains(pos)) {
        _edge = TopRight;
    }
    else if (QRect(rect.left()-border, rect.top(), border*2, border).contains(pos)) {
        _edge = TopLeft;
    }
    else if (QRect(rect.x()-border, rect.y()+border, border*2, rect.height()-border).contains(pos)) {
        _edge = Left;
    }
    else if (QRect(rect.right()-border, rect.y()+border, border*2, rect.height()-border).contains(pos)) {
        _edge = Right;
    }
    else if (QRect(rect.x()+border, rect.bottom()-border, rect.width(), border*2).contains(pos)) {
        _edge = Bottom;
    }
    else if (QRect(rect.x()+border, rect.top()-border, rect.width(), border*2).contains(pos)) {
        _edge = Top;
    }
    else {
        _edge = None;
    }
}
