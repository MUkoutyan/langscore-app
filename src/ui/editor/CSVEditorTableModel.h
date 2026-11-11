#pragma once

#include <QAbstractTableModel>
#include <QString>
#include <vector>
#include <optional>
#include <QSet>
#include <QHash>
#include "FastCSVContainer.h"
#include "../ComponentBase.h"
#include "service/ValidationErrorInfo.h"
#include "../../settings.h"

namespace langscore {

class CSVEditorTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:

    CSVEditorTableModel(QObject* parent = nullptr);
    explicit CSVEditorTableModel(const QString& path, QObject* parent = nullptr);

    bool loadFromEditCSVFile(const QString& path);
    bool loadFromJsonFile(const QString& filePath);
    bool saveToFile(const QString& path);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    size_t rowCountRaw() const;
    size_t colCountRaw() const;
    std::optional<QString> getCell(size_t row, size_t col) const;
    bool setCell(size_t row, size_t col, const QString& value);

    void insertColumn(size_t index, const QString& columnName);
    void removeColumn(int column);

    const std::vector<std::vector<QString>>& dataRaw() const;

    void setSettings(std::shared_ptr<settings> setting);
    void setRuntimeData(std::shared_ptr<ComponentBase::RuntimeData> setting);

    void setUseLanguageFont(bool use);

    bool mergeTranslationsFromCsv(const QString& csvPath);

    QString getCurrentShowFileName() const {
        return currentShowFileName;
    }

    //ローカルとリモートの両方で編集されたセル
    struct MergeConflict {
        int row = 0;
        int column = 0;
        QString mine;   //ローカルの翻訳文
        QString theirs; //リモートの翻訳文
    };
    //saveToFile時に検出した衝突。戻り値のセルはローカルの翻訳文で保存済
    const std::vector<MergeConflict>& mergeConflicts() const { return _mergeConflicts; }

    //ファイルを読み直し、リモートの変更を表に取り込む。
    //取り込んだ際に衝突した場合は戻り値で返す。
    //衝突したセルは resolveConflict で解決するまで衝突した状態で保持する。
    std::vector<MergeConflict> syncWithFile(const QString& path);

    //衝突したセルをどちらの翻訳文にするか決める
    void resolveConflict(const MergeConflict& conflict, bool useTheirs);
    //リモートの翻訳文を読み込んでハイライトしていた箇所をクリアする
    void clearChangedByOthers();
    //リモートの翻訳文を読み込んでハイライトしているセルがあるか
    bool hasChangesByOthers() const { return _changedByOthers.isEmpty() == false; }
    //未解決の衝突が残っているか
    bool hasConflicts() const { return _conflictCells.isEmpty() == false; }

    //保存後に編集したか
    bool isModified() const { return _modified; }
    //保存が必要か。次のいずれかに当てはまる場合は true
    //・編集した
    //・ファイルに無い列がある（後から有効にした言語など）
    //・ファイルに Git の競合マーカーが残っている
    bool needsSave() const;
    //検証後に編集したセルの記録をクリアする。検証の完了時に呼ぶ
    void clearEditedSinceValidation();
    //全言語の翻訳文が埋まっている行の数と、全体の行数を返す
    std::pair<int, int> translatedRowCount() const;

    //表示フィルターで除外されていない検証結果を返す
    //cellErrors はセル単位、fileErrors はファイル全体の結果
    std::vector<const ValidationErrorInfo*> cellErrors(int row, int col) const;
    std::vector<const ValidationErrorInfo*> fileErrors() const;

    void clearAll() {
        beginResetModel();
        csvContainer.clear();
        forgetSavedValues();
        rememberFileContents({});
        _rowsLeftAtLoad.clear();
        _changedByOthers.clear();
        _conflictCells.clear();
        endResetModel();
        emit markStateChanged();
    }


signals:
    void modifiedChanged(bool modified);
    //リモートの翻訳文をセルに読み込んだ
    //対象はリモートのみで変更されたセルと、衝突の解決でリモートを選んだセル
    void changedByOthers(const QString& original, const QString& language, const QString& before, const QString& after);
    //未保存・リモートの読み込み・衝突のいずれかの状態が変化した可能性がある
    //ファイル一覧の表示の更新に使う
    void markStateChanged();

private:
    void setModified(bool modified);
    //保存時の値の記録を破棄する。表を再構築するとセルの位置が変わり、記録が使えなくなるため
    //変更あり状態は保存するまで維持する
    void forgetSavedValues();
    //表示中のファイルの検証結果を返す。無い場合は nullptr
    //必要に応じて _errorIndex を再構築してから返す
    const std::vector<ValidationErrorInfo>* indexedFileErrors() const;
    //セルの書き換えを記録し、未保存状態と検証後に編集したセルを更新する
    void recordEdit(int row, int col, const QString& oldValue, const QString& newValue);

    //---- CSVを共同編集するための処理 ----
    //リモートの変更を消さずに保存するために使う
    //以下、セルのキーは (改行コードを統一した原文, 言語) の組

    //読み書きしたファイルの内容を、リモートの変更を検出するための比較元として保持する
    void rememberFileContents(const FastCSVContainer& file);
    //ファイルからリモートの変更を表へ取り込む
    //ローカルでも編集されたセルは取り込まず、衝突として返す
    //gitConflicts に含まれるセルは、ローカルで未編集でも文章が異なれば衝突として返す
    std::vector<MergeConflict> takeChangesFromFile(const FastCSVContainer& file, const QSet<std::pair<QString, QString>>& gitConflicts);
    //Gitで競合したローカルとリモートの両方を比べ、文章が異なるセルのキーを返す
    //GitConflictSides.baseがある場合、リモートで未変更のセルはsides.theirsをローカルの翻訳文に書き換える
    //GitConflictSides.baseが無い場合、文章が異なるセルをすべて返す
    QSet<std::pair<QString, QString>> mergeGitConflictSides(GitConflictSides& sides) const;
    //保持しているファイルの値を更新し、その値と比べて未保存状態を更新する
    void rememberFileValue(int row, int column, const QString& value);
    //セルをリモートの翻訳文に書き換えてハイライトし、changedByOthers を送出する
    void takeTheirs(int row, int column, const QString& theirs);
    //セルのキーを返す。原文の列が無い場合は空
    std::pair<QString, QString> cellKey(int row, int column) const;
    //表の内容に、ファイルにのみ存在する列・行を加えて返す
    FastCSVContainer withUnknownContents(const FastCSVContainer& file) const;

    langscore::FastCSVContainer csvContainer;
    bool _modified = false;
    //保存後に書き換えたセルの保存時の値。すべて元に戻ると変更あり状態を解除する
    QHash<std::pair<int, int>, QString> _savedValues;
    //forgetSavedValues で記録を破棄した変更があるか。ある場合は保存するまで変更あり状態を維持する
    bool _modifiedWithoutRecord = false;
    //最後に読み直したファイルに Git の競合マーカーがあったか。保存すると消える
    bool _fileHasGitConflictMarkers = false;
    //検証後に編集したセル (行, 列)
    QSet<std::pair<int, int>> _editedSinceValidation;
    //最後に読み書きしたときのファイルの列名
    QStringList _fileHeaders;
    //最後に読み書きしたときのファイルの翻訳文。セルのキー → 翻訳文
    QHash<std::pair<QString, QString>, QString> _fileValues;
    //読み込み時にファイルにあり表に無かった行の原文。古い翻訳へ移した行で、保存時にファイルから削除する
    QSet<QString> _rowsLeftAtLoad;
    //mergeConflicts() が返す衝突したセル
    std::vector<MergeConflict> _mergeConflicts;
    //リモートの翻訳文を読み込んだセル。セルのキー → 読み込む前の翻訳文。ローカルで編集するまでハイライトする
    QHash<std::pair<QString, QString>, QString> _changedByOthers;
    //最後にファイルを読み直したときに検出した、未解決の衝突のセル (行, 列)
    QSet<std::pair<int, int>> _conflictCells;
    QString currentShowFileName;
    bool useLanguageFont = false;
    std::shared_ptr<settings> _settings = nullptr;
    std::shared_ptr<ComponentBase::RuntimeData> _runtimeData = nullptr;
    //検証結果の索引。描画のたびに全件を走査すると結果が多いときに重いため、(行, 言語) で引けるようにする
    struct ErrorIndex {
        quint64 revision = ~0ull;
        size_t size = 0;
        QHash<std::pair<int, QString>, std::vector<size_t>> cells; //(表の行, 言語) → errors の位置
        std::vector<size_t> fileLevel;                             //行と言語を持たない、ファイル全体の結果
    };
    mutable ErrorIndex _errorIndex;
    int _sortedColumn = -1;
    int _sortOrder = 0;
};

} // namespace langscore