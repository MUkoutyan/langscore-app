#pragma once

#include <QTableWidget>
#include <QLabel>
#include <QToolButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QString>

#include <QTextEdit>
#include <vector>

#include "ComponentBase.h"
#include "EditDataManager.h"
#include "editor/CSVEditor.h"
#include "editor/CSVEditorTableModel.h"


struct ScriptTextData; // 前方宣言
class MainCSVTableModel;
class invoker;
class QFileSystemWatcher;
class MainCSVTable : public QWidget, public ComponentBase {
    Q_OBJECT
public:
    MainCSVTable(ComponentBase* component, std::weak_ptr<EditDataManager> loadFileManager, QWidget* parent = nullptr);
    
    void clear();

    void showMainFileText(QString treeItemName, QString fileName);
    void loadAllMainFiles();
    //言語の有効・無効を変えたときに、表示中のファイルに言語の列を足す
    void refreshLanguageColumns();
    //外部エディターで編集したCSVの翻訳を、同じ名前のファイルの表に取り込む。
    //書き換えたセルの数を返す。取り込み先のファイルが無い・CSVを読めない場合は -1
    int importTranslationCsv(const QString& csvPath);
    //保存したときに他の人と同じセルを別の訳にしていたら、どちらの訳にするか選んでもらい、保存し直す
    void resolveMergeConflicts(langscore::CSVEditorTableModel& model, const QString& csvPath);
    //読み込んだすべてのファイルに他の人の変更を取り込み、衝突したセルはどちらの訳にするか選んでもらう（保存の直前に呼ぶ）
    void syncWithAllFiles();
    //他の人の訳を取り込んだ記録の保存先。共有フォルダの他の人の記録と混ざらないよう、プロジェクトごとにアプリのフォルダへ置く
    static QString changeHistoryPath(const QString& langscoreProjectDirectory);
void selectError(quint64 errorId);
    //表示中のファイルで、原文がoriginalの行を緑の枠で示して見える位置までスクロールする。
    //見つからなければ枠を消してfalse
    bool highlightRowByOriginal(const QString& original, const QString& language);
    void clearHighlightRow();
    //表示中のファイルの位置。選んでいるセル（元の表の行・列、無ければ -1）と縦のスクロール位置
    struct ViewPosition { int row = -1; int column = -1; int scroll = 0; };
    ViewPosition viewPosition() const;
    //表示したファイルの位置を戻す。行の高さを合わせてから戻すため、表示の後に呼ぶ
    void restoreViewPosition(const ViewPosition& position);
    //すべてのファイルの進捗を fileProgressChanged で通知する
    void notifyAllFileProgress();

signals:
    void fileProgressChanged(const QString& fileName, int translatedRows, int totalRows);
    //ファイルの一覧に付ける目印。未保存の編集・他の人の訳にしたセル・決めていない衝突があるか
    void fileMarksChanged(const QString& fileName, bool modified, bool changedByOthers, bool hasConflicts);

private:
    enum class ConflictChoice { Mine, Theirs, Later };
    //衝突したセルを1つずつ見せて、どちらの訳にするか選んでもらう。
    //canPostpone なら「あとで決める」を選べる。選ばなかったセルは Later（できない場合は Mine）
    std::vector<ConflictChoice> askConflicts(const QString& csvPath, const langscore::CSVEditorTableModel& model,
                                             const std::vector<langscore::CSVEditorTableModel::MergeConflict>& conflicts, bool canPostpone);
    //読み込んだファイルに他の人の変更を取り込み、衝突したセルはどちらの訳にするか選んでもらう
    void syncWithFile(const QString& csvPath, bool canPostpone);
    //読み込んだファイルのうち、監視していないもの（作り直されたファイルなど）を監視し、変更があったものとして扱う
    void watchLoadedFiles();
    //監視しているファイルが変わったときに呼ぶ。表示中のファイルの衝突はすぐに聞き、他のファイルは表示・保存するときに聞く
    void syncChangedFiles();
    //他の人の訳をセルに取り込んだことを、履歴のファイルに足す（モデルの changedByOthers から呼ぶ）
    void recordChangeByOthers(const QString& original, const QString& language, const QString& before, const QString& after);
    void showChangeHistoryDialog();
    //モデルの markStateChanged から呼び、fileMarksChanged で知らせる
    void notifyFileMarks();

    void selectCell(int row, int column);
    MainCSVTableModel* loadModel(const QString& fileName);
    //解析結果とエディターのCSVから、表の内容を作る
    void fillModelFromFiles(langscore::CSVEditorTableModel& model, const QString& fileName) const;
    std::pair<int, int> fileProgress(const QString& fileName);

    struct TableUndo : QUndoCommand
    {
        using ValueType = bool;
        TableUndo(MainCSVTable* parent, QModelIndex target, ValueType newValue, ValueType oldValue)
            : parent(parent), target(target), newValue(std::move(newValue)), oldValue(std::move(oldValue)){
        }
        ~TableUndo() {}

        int id() const override { return 6; }
        void undo() override;
        void redo() override;

    private:
        MainCSVTable* parent;
        QModelIndex target;
        ValueType newValue;
        ValueType oldValue;

        void setValue(ValueType value);
    };

    void receive(DispatchType type, const QVariantList& args) override;
    void restoreColumnWidths();
    void updateFileNameLabel();
    void updateProgressLabel();
    void updateValidateResultListButton();
    void showOldTranslationsDialog();
    void updateValidateResultTable();
    void refreshValidationView();
    //表示順で、今のセルの次にある検証結果のセルへ移動する
    void selectNextError(bool forward);

    std::weak_ptr<EditDataManager> loadFileManager;
    QLabel* mainFileName;
    QLabel* mainFileWordCount;
    QLineEdit* searchEdit;
    QCheckBox* hideTranslatedCheck;
    QCheckBox* languageFontCheck;
    QCheckBox* fixedWidthCheck;
    QSpinBox* fixedWidthSpinBox;
QPushButton* hideLanguageColumnsAction;
    QPushButton* validateButton;
    QPushButton* validateResultListButton;
    QPushButton* oldTranslationsButton;
    QPushButton* changeHistoryButton;
    QDialog* validateResultDialog;
    QTableWidget* validateResultTable;
    QToolButton* settingButton;
    QWidget* settingPane;
    MainCSVTableModel* currentModel;
    CSVEditor* csvEditor;
    langscore::CSVEditorSortFilterProxyModel* _proxyModel = nullptr;
    QTextEdit* cellErrorLog;
    invoker* _invoker;
    QTimer* updateTimer;
    //他の人が保存した editing のCSVを見つけるための監視
    QFileSystemWatcher* fileWatcher;
    //保存の途中のファイルを読まないよう、変更が落ち着いてから読み直す
    QTimer* fileChangeTimer;
    QSet<QString> changedFiles;
    QString currentFileName;
    QString currentTreeItemName;
    QString currentTreeFileName;
//表示中のモデルとの接続。ファイルを切り替えるときに外す
    QList<QMetaObject::Connection> modelConnections;
bool _finishInvoke;
    bool _showAllScriptContents;
};