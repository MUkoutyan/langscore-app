#pragma once

#include <QWidget>
#include <QMenu>
#include <mutex>
#include <unordered_map>
#include <QFile>
#include <QAbstractTableModel>
#include <QStyledItemDelegate>
#include <QSpinBox>
#include <QComboBox>
#include "ComponentBase.h"
#include "ClipDetectSettingTree.h"

namespace Ui {
class PackingMode;
}


//class CustomDelegate : public QStyledItemDelegate
//{
//    Q_OBJECT
//public:
//    CustomDelegate(QObject* parent = nullptr);
//
//    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
//
//    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
//
//    void setModelData(QWidget* editor, QAbstractItemModel* model, const QModelIndex& index) const override;
//
//    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
//
//signals:
//    void detectModeIndexChanged(const QModelIndex& index, int value) const;
//};

struct ValidationErrorInfo
{
    enum ErrorType { Error, Warning, Invalid };
    enum ErrorSummary {
        None = -1,          //エラーなし
        EmptyCol = 0,       //翻訳文が空
        NotFoundEsc,        //原文にある制御文字が翻訳文に含まれていない
        UnclosedEsc,        //[]で閉じる必要のある制御文字が閉じられていない
        IncludeCR,          //翻訳文にCR改行が含まれている。(マップのみ検出)
        NotEQLang,          //設定した言語とCSVを内の言語列に差異がある
        PartiallyClipped,   //テキストの最後の文字が少し見切れる
        FullyClipped,       //完全に見えない文字がある
        OverTextCount,      //テキストの文字数が多すぎる
        InvalidCSV,         //CSVファイルが不正

        Max
    };

    QString filePath;
    ErrorType type = Invalid;
    ErrorSummary summary = None;
    size_t row = 0;
    int width = 0;
    QString language;
    QString detail;
    size_t id = 0;
    bool shown = false;
};

class PackingCSVTableViewModel : public QAbstractTableModel {
    Q_OBJECT

public:
    explicit PackingCSVTableViewModel(QObject *parent = nullptr);

    ~PackingCSVTableViewModel();

    QString getCurrentCsvFile() const noexcept { return currentFile; }
    void setCsvFile(const QString &filePath, std::vector<std::vector<QString>> contents, QStringList header);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;

    int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void appendErrors(std::vector<ValidationErrorInfo> infos);
    bool canFetchMore(const QModelIndex& parent = QModelIndex()) const override;
    void fetchMore(const QModelIndex& parent = QModelIndex()) override;

signals:
    void requestResizeContents();

private:
    QString readDataAt(int row, int col) const;


private:
    QString currentFile;
    std::vector<ValidationErrorInfo> errors;
    QStringList header;                           // CSV の 1 行目（ヘッダー）
    std::vector<std::vector<QString>> csvContents;      // 現在 QTableView に表示中の行群
    std::vector<std::vector<QString>> fullCsvContents;  // CSV 全体のデータ（ヘッダーを除く）
    int batchSize = 200;                         // 一度に追加する行数
};


class QTreeWidgetItem;
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


    std::unordered_map<QString, std::vector<ValidationErrorInfo>> errors;
    std::unordered_map<QString, bool> updateList;
    std::unordered_map<QString, QTreeWidgetItem*> treeTopItemList;
    std::mutex _mutex;
    PackingCSVTableViewModel* csvTableViewModel;
    ClipDetectSettingTree* clipDetectSettingTree;
    QTimer* updateTimer;
    bool _finishInvoke;
    size_t errorInfoIndex;
    QString currentShowCSV;
    bool isValidate;
    bool showLog;
    bool suspendResizeToContents;

    QIcon attentionIcon;
    QIcon warningIcon;

    QMenu* treeMenu;
    QString lastSelectedInvalidCSVPath;


    QString getCurrentSelectedItemFilePath();
    void setupCsvTable(QString filePath);
    void setupCsvTree();
    void highlightError(QTreeWidgetItem* item);
    std::vector<ValidationErrorInfo> processJsonBuffer(const QString& input);

    void showEvent(QShowEvent *event) override;

    bool eventFilter(QObject* obj, QEvent* event) override;
    void copySelectedCell();

private slots:
    void validate();
    void packing();

    void addErrorText(QString text);
    void updateTree();


    void setPackingSourceDir(QString path);
    void resizeCsvTable();

private:

    //キーはファイルパス
    ValidationErrorInfo convertErrorInfo(std::vector<QString> csvText);
};

