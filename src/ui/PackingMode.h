#pragma once

#include <QWidget>
#include "ComponentBase.h"

namespace Ui {
class PackingMode;
}

class invoker;
class PackingMode : public QWidget, public ComponentBase
{
    Q_OBJECT

public:
    explicit PackingMode(ComponentBase* setting, QWidget *parent = nullptr);
    ~PackingMode();

signals:
    void toPrevPage();

private:
    Ui::PackingMode *ui;
    invoker* _invoker;

private slots:
    void validate();
    void packing();
};

