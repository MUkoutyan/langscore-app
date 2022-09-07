#ifndef FORMTASKBAR_H
#define FORMTASKBAR_H

#include <QWidget>
#include <QUndoStack>
#include <QMenu>

namespace Ui {
class FormTaskBar;
}

class FormTaskBar : public QWidget
{
    Q_OBJECT

public:

    enum Theme {
        Dark,
        Light,
        System
    };

    explicit FormTaskBar(QUndoStack* history, QWidget *parent = nullptr);
    ~FormTaskBar();

signals:
    void pushClose();
    bool maximum();
    void minimum();
    void doubleClick();

    void saveProj();
    void saveAsProj();
    void requestOpenProj(QString path);
    void quit();

    void undo();
    void redo();
    void showUndoView();

    void changeTheme(Theme);

    void dragging(QMouseEvent* event, QPoint delta);

public slots:
    void updateRecentMenu();
    void emitChangeTheme(Theme);

private:
    Ui::FormTaskBar *ui;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;


    bool pressLeftButton;
    QPoint beforeMousePos;
    QUndoStack* history;

    QMenu* recentProjMenu;
};

#endif // FORMTASKBAR_H
