#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QGraphicsPixmapItem>
#include "ComponentBase.h"

namespace Ui {
class WriteModeComponent;
}

class MainComponent;
class WriteModeComponent : public QWidget, public ComponentBase
{
    Q_OBJECT
public:
    explicit WriteModeComponent(Common::Type setting, MainComponent *parent);
    ~WriteModeComponent() override;

    void show();

signals:

private:
    Ui::WriteModeComponent* ui;
    MainComponent* _parent;
    QGraphicsScene* scene;

    void setNormalCsvText(QString fileName);
    void setScriptCsvText();

};

