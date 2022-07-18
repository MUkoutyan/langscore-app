#pragma once

#include <QWidget>
#include <QTableWidget>
#include <QGraphicsPixmapItem>
#include "ComponentBase.h"

namespace Ui {
class WriteModeComponent;
}

class LanguageSelectComponent;
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
    std::vector<LanguageSelectComponent*> languageButtons;

    void setNormalCsvText(QString fileName);
    void setScriptCsvText();

    QTableWidgetItem* scriptTableItem(int row, int col);

};

