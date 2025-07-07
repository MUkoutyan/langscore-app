#pragma once

#include <QString>
#include <QStringList>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <stdexcept>

namespace langscore {

class FastCSVContainer {
public:
    using RowIndex = size_t;
    using ColumnIndex = size_t;
    using CellData = QString;

    FastCSVContainer() = default;
    ~FastCSVContainer() = default;

    // コピー・ムーブコンストラクタ
    FastCSVContainer(const FastCSVContainer&) = default;
    FastCSVContainer(FastCSVContainer&&) = default;
    FastCSVContainer& operator=(const FastCSVContainer&) = default;
    FastCSVContainer& operator=(FastCSVContainer&&) = default;

    // データ初期化
    bool loadFromCsvData(const std::vector<std::vector<CellData>>& csvData);
    bool loadFromFile(const CellData& filePath);
    bool saveToFile(const CellData& filePath) const;
    CellData unescapeCSVField(const CellData& field) const;
    CellData escapeCSVField(const CellData& field) const;
    void clear();

    // 基本アクセス
    inline const CellData& at(RowIndex row, ColumnIndex col) const;
    inline const CellData& at(RowIndex row, const CellData& columnName) const;
    inline CellData& at(RowIndex row, ColumnIndex col);
    inline CellData& at(RowIndex row, const CellData& columnName);

    // 安全アクセス（範囲チェック付き）
    bool getValue(RowIndex row, ColumnIndex col, CellData& out) const;
    bool getValue(RowIndex row, const CellData& columnName, CellData& out) const;
    bool setValue(RowIndex row, ColumnIndex col, const CellData& value);
    bool setValue(RowIndex row, const CellData& columnName, const CellData& value);

    // 行・列操作
    void addRow(QStringList rowData);
    void addRow(std::vector<CellData> rowData);
    void insertRow(RowIndex index, std::vector<CellData> rowData);
    void removeRow(RowIndex index);
    
    void addColumn(const CellData& columnName, const CellData& defaultValue = "");
    void insertColumn(ColumnIndex index, const CellData& columnName, const CellData& defaultValue = "");
    void removeColumn(ColumnIndex index);
    void removeColumn(const CellData& columnName);

    // 行・列の取得
    QStringList getRow(RowIndex row) const;
    QStringList getColumn(ColumnIndex col) const;
    QStringList getColumn(const CellData& columnName) const;

    // 検索機能
    std::vector<RowIndex> findRowsWithValue(ColumnIndex col, const CellData& value) const;
    std::vector<RowIndex> findRowsWithValue(const CellData& columnName, const CellData& value) const;
    std::vector<RowIndex> findRowsWithValueContaining(ColumnIndex col, const CellData& substring) const;
    std::vector<RowIndex> findRowsWithValueContaining(const CellData& columnName, const CellData& substring) const;

    // サイズ・情報取得
    inline size_t rowCount() const noexcept { return data_.size(); }
    inline size_t columnCount() const noexcept { return headers_.size(); }
    inline bool isEmpty() const noexcept { return data_.empty() || headers_.empty(); }

    // ヘッダー関連
    inline const QStringList& headers() const noexcept { return headers_; }
    inline const CellData& headerAt(ColumnIndex col) const;
    inline ColumnIndex getColumnIndex(const CellData& columnName) const;
    inline bool hasColumn(const CellData& columnName) const;

    // Qt互換性
    QStringList toQStringListRow(RowIndex row) const { return getRow(row); }
    std::vector<QStringList> toQStringListVector() const;

private:
    QStringList headers_;
    std::vector<std::vector<CellData>> data_;
    std::unordered_map<CellData, ColumnIndex> columnNameToIndex_;

    // インデックス検索用（オプション：大きなデータセット用）
    mutable std::unordered_map<ColumnIndex, std::unordered_map<CellData, std::vector<RowIndex>>> columnValueIndex_;
    mutable bool indexBuilt_ = false;

    void buildColumnIndex() const;
    void updateColumnIndex(RowIndex row, ColumnIndex col, const CellData& oldValue, const CellData& newValue) const;
    void validateRowColumn(RowIndex row, ColumnIndex col) const;
    void rebuildColumnNameMap();
};

// インライン実装
inline const FastCSVContainer::CellData& FastCSVContainer::at(RowIndex row, ColumnIndex col) const {
    validateRowColumn(row, col);
    return data_[row][col];
}

inline const FastCSVContainer::CellData& FastCSVContainer::at(RowIndex row, const CellData& columnName) const {
    return at(row, getColumnIndex(columnName));
}

inline FastCSVContainer::CellData& FastCSVContainer::at(RowIndex row, ColumnIndex col) {
    validateRowColumn(row, col);
    return data_[row][col];
}

inline FastCSVContainer::CellData& FastCSVContainer::at(RowIndex row, const CellData& columnName) {
    return at(row, getColumnIndex(columnName));
}

inline const FastCSVContainer::CellData& FastCSVContainer::headerAt(ColumnIndex col) const {
    if (col >= headers_.size()) {
        throw std::out_of_range("Column index out of range");
    }
    return headers_[static_cast<int>(col)];
}

inline FastCSVContainer::ColumnIndex FastCSVContainer::getColumnIndex(const CellData& columnName) const {
    auto it = columnNameToIndex_.find(columnName);
    if (it == columnNameToIndex_.end()) {
        throw std::invalid_argument(CellData("Column '%1' not found").arg(columnName).toStdString());
    }
    return it->second;
}

inline bool FastCSVContainer::hasColumn(const CellData& columnName) const {
    return columnNameToIndex_.find(columnName) != columnNameToIndex_.end();
}

} // namespace langscore