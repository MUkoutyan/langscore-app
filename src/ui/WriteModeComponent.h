#pragma once

#include <QWidget>
#include <QTreeWidgetItem>
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

public slots:
    void treeItemSelected();
    void treeItemChanged(QTreeWidgetItem *_item, int column);
    void scriptTableSelected();
    void scriptTableItemChanged(QTableWidgetItem *item);

private:
    Ui::WriteModeComponent* ui;
    MainComponent* _parent;
    QGraphicsScene* scene;
    std::vector<LanguageSelectComponent*> languageButtons;
    bool showAllScriptContents;

    void setNormalCsvText(QString fileName);
    void setScriptCsvText();
    void showScriptFile(QString scriptFilePath);

    QTableWidgetItem* scriptTableItem(int row, int col);

};

