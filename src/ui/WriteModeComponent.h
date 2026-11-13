#pragma once

#include <QWidget>
#include <QPointer>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QGraphicsPixmapItem>

#include <QPainter>
#include <QResizeEvent>

#include "ComponentBase.h"
#include "EditDataManager.h"
#include "FileTree.h"
#include "MainCSVTable.h"
#include "ScriptCSVTable.h"

#include <variant>

class QPushButton;

namespace Ui {
class WriteModeComponent;
}

class ScriptViewer;
class ClipDetectSettingTree;
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
    void loadAllMainFiles();
    //外部エディターで編集した翻訳のCSVをエディターに取り込み、結果の文章を返す
    QString importTranslationCsvFiles(const QStringList& csvPaths);
    void updateExportDirectory();

public slots:
    void treeItemSelected();
    void treeItemChanged(QTreeWidgetItem *_item, int column);
    void scriptTableSelected(QString scriptName, QString scriptFilePath, QString textPoint, int textLen);
    //void scriptTableItemChanged(QTableWidgetItem *item);

    void exportPlugin();
    void testPlay();
    void exportTranslateFiles();
    void onScriptTreeItemCheckChanged(QString scriptName, Qt::CheckState check);
    void onScriptTableItemCheckChanged(QString scriptName, Qt::CheckState check);

private slots:
    // FileTreeからのsignalを受けるslot
    void onShowGraphicsImage(const QString& imagePath);
    void onShowMainFile(const QString& treeItemName, const QString& fileName);
    void onShowScriptFile(const QString& scriptFilePath);
    void onSetScriptFileNameLabel(const QString& label);
    void onHighlightScriptText(int row, int col, int length);
    void onSetTabIndex(int index);

private:
#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif

    enum InvokeType {
        None,
        ExportCSV,
        UpdatePlugin,
        Reanalisys,
        PackingAfterExport
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

    //最後に表示していたファイル・タブ・表の位置。プロジェクトごとにアプリの設定へ保存し、開き直したときに復元する
    QString viewStateGroup;
    void saveViewState();
    void restoreViewState();

    void backup();

    void setFontList(std::vector<QString> fontPaths);

    void changeEnabledUIState(bool enable);
    //プラグインの追加前は追加ボタンのみ、追加後はテストプレイ・書き出しなどのボタンを表示する
    void updateExportButtonsVisible();

    void receive(DispatchType type, const QVariantList& args) override;

    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;

    bool confirmOverwriteReadOnlyPlugins();

    void applyEditingTranslationsToExport();
    void exportCsvToExportDirectory();
    void finishExport(bool openExportFolder = true);
    void launchTestPlay();
    void showLogTab();
    void restoreTabBeforeLog();
    //ログのタブを表示する前のタブ。処理が成功したらこのタブに戻す
    QPointer<QWidget> tabBeforeLog;
    //テストプレイ前の書き出し中は true。書き出しの完了後にゲームを起動する
    bool testPlayAfterExport = false;

    void changeUIColor();

    //----RemoteHub----
    void setupRemoteHub();
    void updateFollowGameButton();
    void setHasUnappliedEdits(bool hasEdits);
    void updateApplyToGameButton();
    QPushButton* followGameButton = nullptr;
    QPushButton* applyToGameButton = nullptr;
    //ゲームへの反映・上書き保存での書き出しの場合、書き出し先のフォルダーを開かない
    bool applyToGameAfterExport = false;
    //ゲームに反映していない編集があるか
    bool hasUnappliedEdits = false;
    //テストプレイ中のゲームが起動しているか
    bool isGameRunning = false;

    std::shared_ptr<EditDataManager> csvEditDataManager;
    FileTree* fileTree;
    MainCSVTable* mainTable;
    ScriptCSVTable* scriptTable;
    ClipDetectSettingTree* clipDetectSettingTree;
};

