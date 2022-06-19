#ifndef FORMTASKBAR_H
#define FORMTASKBAR_H

#include <QWidget>

namespace Ui {
class FormTaskBar;
}

class FormTaskBar : public QWidget
{
    Q_OBJECT

public:
    explicit FormTaskBar(QWidget *parent = nullptr);
    ~FormTaskBar();

signals:
    void pushClose();
    void maximum();
    void minimum();
    void doubleClick();

    void openGameProj();
    void saveProj();
    void saveAsProj();
    void requestOpenProj(QString path);
    void quit();

    void dragging(QMouseEvent* event, QPoint delta);

private:
    Ui::FormTaskBar *ui;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    bool pressLeftButton;
    QPoint beforeMousePos;
};

#endif // FORMTASKBAR_H
