#pragma once

#include <QWidget>

namespace Ui {
class WriteModeComponent;
}

class MainComponent;
class WriteModeComponent : public QWidget
{
    Q_OBJECT
public:
    explicit WriteModeComponent(MainComponent *parent);

    void show();

signals:

private:
    Ui::WriteModeComponent* ui;
    MainComponent* _parent;

};

