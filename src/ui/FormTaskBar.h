#ifndef FORMTASKBAR_H
#define FORMTASKBAR_H

#include <QPushButton>
#include <QWidget>
#include <QUndoStack>
#include <QMenu>

#include "ComponentBase.h"
#include "ColorTheme.h"

namespace Ui {
class FormTaskBar;
}

class FormTaskBar : public QWidget, public ComponentBase
{
    Q_OBJECT

public:


    explicit FormTaskBar(QUndoStack* history, ComponentBase* setting, QWidget *parent = nullptr);
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

    void changeTheme(ColorTheme::Theme);

    void dragging(QMouseEvent* event, QPoint delta);

public slots:
    void updateRecentMenu();
    void emitChangeTheme(ColorTheme::Theme);

private:
    Ui::FormTaskBar *ui;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

    void SetIconWithReverceColor(QPushButton* button, QString iconPath);


    bool pressLeftButton;
    QPoint beforeMousePos;
    QUndoStack* history;

    QMenu* recentProjMenu;
};

#endif // FORMTASKBAR_H
