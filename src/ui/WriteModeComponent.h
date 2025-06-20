#pragma once

#include <QWidget>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QGraphicsPixmapItem>

#include <QPainter>
#include <QResizeEvent>

#include "ComponentBase.h"
#include "LoadFileManager.h"
#include "FileTree.h"
#include "MainCSVTable.h"
#include "ScriptCSVTable.h"

#include <variant>

namespace Ui {
class WriteModeComponent;
}

class ScriptViewer;
class LanguageSelectComponent;
class invoker;
class WriteModeComponent : public QWidget, public ComponentBase
{
    Q_OBJECT
public:
    explicit WriteModeComponent(ComponentBase* setting, QWidget *parent);
    ~WriteModeComponent() override;

    void show();
    void clear();

signals:
    void toNextPage();

public slots:
    void treeItemSelected();
    void treeItemChanged(QTreeWidgetItem *_item, int column);
    void scriptTableSelected(QString scriptName, QString scriptFilePath, size_t textRow, size_t textCol, int textLen);
    //void scriptTableItemChanged(QTableWidgetItem *item);

    void exportPlugin();
    void exportTranslateFiles();
    void firstExportTranslateFiles();
    void onScriptTreeItemCheckChanged(QString scriptName, Qt::CheckState check); // 追加

private slots:
    // FileTreeからのsignalを受けるslot
    void onShowGraphicsImage(const QString& imagePath);
    void onShowMainFile(const QString& treeItemName, const QString& fileName);
    void onShowScriptFile(const QString& scriptFilePath);
    void onSetScriptFileNameLabel(const QString& label);
    void onHighlightScriptText(int row, int col, int length);
    void onSetTabIndex(int index);

private:

    enum InvokeType {
        None,
        ExportFirstTime,
        ExportCSV,
        UpdatePlugin,
        Reanalisys
    };

    Ui::WriteModeComponent* ui;
    QGraphicsScene* scene;
    std::vector<LanguageSelectComponent*> languageButtons;
    int currentScriptWordCount;
    bool showAllScriptContents;
    invoker* _invoker;
    InvokeType invokeType;
    QString lastWritePath;
    ScriptViewer* scriptViewer;


    void setup();
    void setupTree();

    void setScriptTableItemCheck(QTableWidgetItem *_item, Qt::CheckState check);

    void writeToIgnoreScriptLine(int row, bool ignore);

    void backup();

    void setFontList(std::vector<QString> fontPaths);

    void changeEnabledUIState(bool enable);

    void receive(DispatchType type, const QVariantList& args) override;

    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif

    void changeUIColor();

    std::shared_ptr<LoadFileManager> loadFileManager;
    FileTree* fileTree;
    MainCSVTable* mainTable;
    ScriptCSVTable* scriptTable;
};

