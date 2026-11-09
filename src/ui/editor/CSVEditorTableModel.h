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

    //保存したときに、他の人が同じセルを別の訳に変えていたセル。ファイルには自分の訳で保存している
    struct MergeConflict {
        int row = 0;
        int column = 0;
        QString mine;
        QString theirs;
    };
    //最後の保存で起きた衝突
    const std::vector<MergeConflict>& mergeConflicts() const { return _mergeConflicts; }
    //他の人が保存したファイルを読み直し、他の人だけが変えたセルを表へ取り込む。
    //両方が別の訳に変えたセルは、表もそのままにして返す。resolveConflict で決めるまで、読み直すたびに返す
    std::vector<MergeConflict> syncWithFile(const QString& path);
    //衝突したセルをどちらの訳にするか決める。決めた後は、そのセルを衝突として扱わない
    void resolveConflict(const MergeConflict& conflict, bool useTheirs);
    //他の人の訳にしたセルの印を消す（自分で編集したセルの印は、編集したときに消える）
    void clearChangedByOthers();
    //他の人の訳にしたセルがあるか
    bool hasChangesByOthers() const { return _changedByOthers.isEmpty() == false; }
    //他の人の保存と衝突して、まだ決めていないセルがあるか
    bool hasConflicts() const { return _conflictCells.isEmpty() == false; }

    //保存してから編集されたか
    bool isModified() const { return _modified; }
    //保存が必要か。編集したか、ファイルに無い列（後から有効にした言語など）がある
    bool needsSave() const;
    //検証の後に編集したセルの記録を消す（検証が終わったときに呼ぶ）
    void clearEditedSinceValidation();
    //すべての言語が埋まっている行の数と、全体の行数
    std::pair<int, int> translatedRowCount() const;

    //セル・ファイル全体の検証結果のうち、表示の絞り込みで隠していないもの
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
    //他の人の訳をセルに取り込んだ（他の人だけが変えたセル・衝突で他の人の訳を選んだセル）
    void changedByOthers(const QString& original, const QString& language, const QString& before, const QString& after);
    //未保存・他の人の訳にしたセル・衝突の有無が変わったかもしれない（ファイルの一覧の目印を付け直す）
    void markStateChanged();

private:
    void setModified(bool modified);
    //表を作り直すとセルの位置が変わるため、保存したときの値の記録を捨てる（変更ありのまま残す）
    void forgetSavedValues();
    //表示中のファイルの検証結果。セルの索引を必要なら作り直してから返す。無ければ nullptr
    const std::vector<ValidationErrorInfo>* indexedFileErrors() const;
    //セルを書き換えたことを記録する（未保存の印・検証の後に編集したセル）
    void recordEdit(int row, int col, const QString& oldValue, const QString& newValue);

    //複数人で同じ editing のCSVを共有したときに、他の人の訳を消さずに保存するための処理
    //読み書きしたファイルの内容を、他の人の変更と比べる元として覚える
    void rememberFileContents(const FastCSVContainer& file);
    //他の人だけが変えたセルを表へ取り込む。自分も変えていて訳が違うセルは、衝突として返す
    std::vector<MergeConflict> takeChangesFromFile(const FastCSVContainer& file);
    //セルのファイルの値を覚え直す。未保存の印も、ファイルの値と比べて付け直す
    void rememberFileValue(int row, int column, const QString& value);
    //セルを他の人の訳にし、印を付けて changedByOthers で知らせる
    void takeTheirs(int row, int column, const QString& theirs);
    //(改行をそろえた原文, 言語)。原文の列が無ければ空
    std::pair<QString, QString> cellKey(int row, int column) const;
    //表の内容に、表に無い列・行をファイルから足したもの
    FastCSVContainer withUnknownContents(const FastCSVContainer& file) const;

    langscore::FastCSVContainer csvContainer;
    bool _modified = false;
    //保存してから書き換えたセルの、保存したときの値。すべて元に戻すと「変更あり」を解除する
    QHash<std::pair<int, int>, QString> _savedValues;
    //記録を捨てた変更があるか（forgetSavedValues）。あれば保存するまで「変更あり」のままにする
    bool _modifiedWithoutRecord = false;
    QSet<std::pair<int, int>> _editedSinceValidation;
    //最後に読み書きしたときのファイルの列と訳。(改行をそろえた原文, 言語) → 訳
    QStringList _fileHeaders;
    QHash<std::pair<QString, QString>, QString> _fileValues;
    //読み込んだときにファイルにあり、表に無かった行の原文（古い翻訳へ移した行）。保存で消す
    QSet<QString> _rowsLeftAtLoad;
    std::vector<MergeConflict> _mergeConflicts;
    //他の人の訳にしたセル。(改行をそろえた原文, 言語) → 変わる前の訳。自分で編集するまで印を付ける
    QHash<std::pair<QString, QString>, QString> _changedByOthers;
    //他の人の保存と衝突して、まだ決めていないセル（最後に読み直したときの衝突）
    QSet<std::pair<int, int>> _conflictCells;
    QString currentShowFileName;
    bool useLanguageFont = false;
    std::shared_ptr<settings> _settings = nullptr;
    std::shared_ptr<ComponentBase::RuntimeData> _runtimeData = nullptr;
    //セルを描くたびに全件を調べると、結果が多いときに重くなるため、(行, 言語) から引けるようにしておく
    struct ErrorIndex {
        quint64 revision = ~0ull;
        size_t size = 0;
        QHash<std::pair<int, QString>, std::vector<size_t>> cells; //(表の行, 言語) → errors の位置
        std::vector<size_t> fileLevel;                             //行も言語も無い、ファイル全体の結果
    };
    mutable ErrorIndex _errorIndex;
    int _sortedColumn = -1;
    int _sortOrder = 0;
};

} // namespace langscore