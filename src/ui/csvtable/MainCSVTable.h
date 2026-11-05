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


struct ScriptTextData; // 前方宣言
class MainCSVTableModel;
class invoker;
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
void selectError(quint64 errorId);
    //すべてのファイルの進捗を fileProgressChanged で通知する
    void notifyAllFileProgress();

signals:
    void fileProgressChanged(const QString& fileName, int translatedRows, int totalRows);

private:
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
    QString currentFileName;
    QString currentTreeItemName;
    QString currentTreeFileName;
//表示中のモデルとの接続。ファイルを切り替えるときに外す
    QList<QMetaObject::Connection> modelConnections;
bool _finishInvoke;
    bool _showAllScriptContents;
};