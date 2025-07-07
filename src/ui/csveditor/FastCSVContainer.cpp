#include "FastCSVContainer.h"
#include "../../csv.hpp"
#include <algorithm>
#include <stdexcept>
#include <QFile>
#include <QTextStream>

namespace langscore {

bool FastCSVContainer::loadFromCsvData(const std::vector<std::vector<CellData>>& csvData) 
{
    clear();
    
    if (csvData.empty()) {
        return false;
    }

    // ヘッダーを設定
    const auto& headerRow = csvData[0];
    headers_ = QStringList(headerRow.begin(), headerRow.end());
    rebuildColumnNameMap();

    // データ行を設定
    data_.reserve(csvData.size() - 1);
    for (size_t i = 1; i < csvData.size(); ++i) {
        auto row = csvData[i];
        std::transform(row.begin(), row.end(), row.begin(), [this](const auto& field) { return unescapeCSVField(field); });
        data_.emplace_back(row);
        
        // 列数が足りない場合は空文字で埋める
        if (data_.back().size() < headers_.size()) {
            data_.back().resize(headers_.size(), "");
        }
    }

    indexBuilt_ = false;
    return true;
}

bool FastCSVContainer::loadFromFile(const CellData& filePath) {
    auto csvData = langscore::readCsv(filePath);
    return loadFromCsvData(csvData);
}

bool FastCSVContainer::saveToFile(const CellData& filePath) const
{
    // データが空の場合はfalseを返す
    if(isEmpty()) {
        return false;
    }

    // ファイルを開く
    QFile file(filePath);
    if(!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // ヘッダー行を書き出し
    for(int i = 0; i < headers_.size(); ++i) {
        if(i > 0) {
            stream << ",";
        }
        stream << escapeCSVField(headers_[i]);
    }
    stream << "\n";

    // データ行を書き出し
    for(const auto& row : data_) {
        for(size_t col = 0; col < headers_.size(); ++col) {
            if(col > 0) {
                stream << ",";
            }

            CellData value;
            if(col < row.size()) {
                value = row[col];
            }
            // 空文字の場合はそのまま出力
            stream << escapeCSVField(value);
        }
        stream << "\n";
    }

    return stream.status() == QTextStream::Ok;
}

FastCSVContainer::CellData FastCSVContainer::unescapeCSVField(const CellData& field) const
{
    // 空文字またはダブルクォートで囲まれていない場合はそのまま返す
    if(field.isEmpty() || field.startsWith('"') == false || field.endsWith('"') == false) {
        return field;
    }

    // 長さが2未満（""のみ）の場合は空文字を返す
    if(field.length() < 2) {
        return "";
    }

    // 前後のダブルクォートを除去
    CellData unescaped = field.mid(1, field.length() - 2);

    // エスケープされたダブルクォート（""）を元に戻す（"に変換）
    unescaped.replace("\"\"", "\"");

    return unescaped;
}

FastCSVContainer::CellData FastCSVContainer::escapeCSVField(const CellData& field) const
{
    // フィールドにカンマ、ダブルクォート、改行が含まれている場合はダブルクォートで囲む
    if(field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        CellData escaped = field;
        // ダブルクォートをエスケープ（""に変換）
        escaped.replace("\"", "\"\"");
        return "\"" + escaped + "\"";
    }
    return field;
}

void FastCSVContainer::clear() {
    headers_.clear();
    data_.clear();
    columnNameToIndex_.clear();
    columnValueIndex_.clear();
    indexBuilt_ = false;
}

bool FastCSVContainer::getValue(RowIndex row, ColumnIndex col, CellData& out) const {
    if (row >= data_.size() || col >= headers_.size()) {
        return false;
    }
    if (col >= data_[row].size()) {
        out = "";
        return true;
    }
    out = data_[row][col];
    return true;
}

bool FastCSVContainer::getValue(RowIndex row, const CellData& columnName, CellData& out) const {
    auto it = columnNameToIndex_.find(columnName);
    if (it == columnNameToIndex_.end()) {
        return false;
    }
    return getValue(row, it->second, out);
}

bool FastCSVContainer::setValue(RowIndex row, ColumnIndex col, const CellData& value) {
    if (row >= data_.size() || col >= headers_.size()) {
        return false;
    }
    
    if (col >= data_[row].size()) {
        data_[row].resize(headers_.size(), "");
    }
    
    const auto oldValue = data_[row][col];
    data_[row][col] = value;
    
    if (indexBuilt_) {
        updateColumnIndex(row, col, oldValue, value);
    }
    
    return true;
}

bool FastCSVContainer::setValue(RowIndex row, const CellData& columnName, const CellData& value) {
    auto it = columnNameToIndex_.find(columnName);
    if (it == columnNameToIndex_.end()) {
        return false;
    }
    return setValue(row, it->second, value);
}

void FastCSVContainer::addRow(QStringList rowData) {
    addRow(std::vector<CellData>{rowData.begin(), rowData.end()});
}

void FastCSVContainer::addRow(std::vector<CellData> rowData) {
    data_.emplace_back(std::move(rowData));
    if (data_.back().size() < headers_.size()) {
        data_.back().resize(headers_.size(), "");
    }
    
    if (indexBuilt_) {
        const RowIndex newRow = data_.size() - 1;
        for (ColumnIndex col = 0; col < headers_.size(); ++col) {
            if (col < data_[newRow].size()) {
                updateColumnIndex(newRow, col, "", data_[newRow][col]);
            }
        }
    }
}

void FastCSVContainer::insertRow(RowIndex index, std::vector<CellData> rowData) {
    if (index > data_.size()) {
        addRow(std::move(rowData));
        return;
    }
    
    if (rowData.size() < headers_.size()) {
        rowData.resize(headers_.size(), "");
    }
    
    data_.insert(data_.begin() + index, rowData);
    indexBuilt_ = false; // インデックスを再構築が必要
}

void FastCSVContainer::removeRow(RowIndex index) {
    if (index >= data_.size()) {
        return;
    }
    
    data_.erase(data_.begin() + index);
    indexBuilt_ = false; // インデックスを再構築が必要
}

void FastCSVContainer::addColumn(const CellData& columnName, const CellData& defaultValue) 
{
    if(headers_.contains(columnName)) {
        return;
    }

    headers_.append(columnName);
    columnNameToIndex_[columnName] = headers_.size() - 1;
    
    for (auto& row : data_) {
        row.push_back(defaultValue);
    }
    
    indexBuilt_ = false;
}

void FastCSVContainer::insertColumn(ColumnIndex index, const CellData& columnName, const CellData& defaultValue) {
    if (index >= headers_.size()) {
        addColumn(columnName, defaultValue);
        return;
    }
    
    if(headers_.contains(columnName)) {
        return;
    }

    headers_.insert(index, columnName);
    rebuildColumnNameMap();
    
    for (auto& row : data_) {
        row.insert(row.begin() + index, defaultValue);
    }
    
    indexBuilt_ = false;
}

void FastCSVContainer::removeColumn(ColumnIndex index) {
    if (index >= headers_.size()) {
        return;
    }
    
    headers_.removeAt(static_cast<int>(index));
    rebuildColumnNameMap();
    
    for (auto& row : data_) {
        if (index < row.size()) {
            row.erase(row.begin() + index);
        }
    }
    
    indexBuilt_ = false;
}

void FastCSVContainer::removeColumn(const CellData& columnName) {
    auto it = columnNameToIndex_.find(columnName);
    if (it != columnNameToIndex_.end()) {
        removeColumn(it->second);
    }
}

QStringList FastCSVContainer::getRow(RowIndex row) const {
    if (row >= data_.size()) {
        return QStringList();
    }
    
    QStringList result;
    const auto& rowData = data_[row];
    for (size_t i = 0; i < headers_.size(); ++i) {
        if (i < rowData.size()) {
            result.append(rowData[i]);
        } else {
            result.append("");
        }
    }
    return result;
}

QStringList FastCSVContainer::getColumn(ColumnIndex col) const {
    if (col >= headers_.size()) {
        return QStringList();
    }
    
    QStringList result;
    result.reserve(static_cast<int>(data_.size()));
    
    for (const auto& row : data_) {
        if (col < row.size()) {
            result.append(row[col]);
        } else {
            result.append("");
        }
    }
    return result;
}

QStringList FastCSVContainer::getColumn(const CellData& columnName) const {
    auto it = columnNameToIndex_.find(columnName);
    if (it == columnNameToIndex_.end()) {
        return QStringList();
    }
    return getColumn(it->second);
}

std::vector<FastCSVContainer::RowIndex> FastCSVContainer::findRowsWithValue(ColumnIndex col, const CellData& value) const {
    if (!indexBuilt_) {
        buildColumnIndex();
    }
    
    auto colIt = columnValueIndex_.find(col);
    if (colIt == columnValueIndex_.end()) {
        return {};
    }
    
    auto valueIt = colIt->second.find(value);
    if (valueIt == colIt->second.end()) {
        return {};
    }
    
    return valueIt->second;
}

std::vector<FastCSVContainer::RowIndex> FastCSVContainer::findRowsWithValue(const CellData& columnName, const CellData& value) const {
    auto it = columnNameToIndex_.find(columnName);
    if (it == columnNameToIndex_.end()) {
        return {};
    }
    return findRowsWithValue(it->second, value);
}

std::vector<FastCSVContainer::RowIndex> FastCSVContainer::findRowsWithValueContaining(ColumnIndex col, const CellData& substring) const {
    std::vector<RowIndex> result;
    
    if (col >= headers_.size()) {
        return result;
    }
    
    for (RowIndex row = 0; row < data_.size(); ++row) {
        if (col < data_[row].size() && data_[row][col].contains(substring, Qt::CaseInsensitive)) {
            result.push_back(row);
        }
    }
    
    return result;
}

std::vector<FastCSVContainer::RowIndex> FastCSVContainer::findRowsWithValueContaining(const CellData& columnName, const CellData& substring) const {
    auto it = columnNameToIndex_.find(columnName);
    if (it == columnNameToIndex_.end()) {
        return {};
    }
    return findRowsWithValueContaining(it->second, substring);
}

std::vector<QStringList> FastCSVContainer::toQStringListVector() const {
    std::vector<QStringList> result;
    result.reserve(data_.size());
    
    for (RowIndex row = 0; row < data_.size(); ++row) {
        result.push_back(getRow(row));
    }
    
    return result;
}

void FastCSVContainer::buildColumnIndex() const {
    columnValueIndex_.clear();
    
    for (ColumnIndex col = 0; col < headers_.size(); ++col) {
        for (RowIndex row = 0; row < data_.size(); ++row) {
            if (col < data_[row].size()) {
                const auto& value = data_[row][col];
                columnValueIndex_[col][value].push_back(row);
            }
        }
    }
    
    indexBuilt_ = true;
}

void FastCSVContainer::updateColumnIndex(RowIndex row, ColumnIndex col, const CellData& oldValue, const CellData& newValue) const {
    if (!indexBuilt_) {
        return;
    }
    
    auto colIt = columnValueIndex_.find(col);
    if (colIt == columnValueIndex_.end()) {
        return;
    }
    
    // 古い値から行インデックスを削除
    auto oldValueIt = colIt->second.find(oldValue);
    if (oldValueIt != colIt->second.end()) {
        auto& rows = oldValueIt->second;
        rows.erase(std::remove(rows.begin(), rows.end(), row), rows.end());
        if (rows.empty()) {
            colIt->second.erase(oldValueIt);
        }
    }
    
    // 新しい値に行インデックスを追加
    colIt->second[newValue].push_back(row);
}

void FastCSVContainer::validateRowColumn(RowIndex row, ColumnIndex col) const {
    if (row >= data_.size()) {
        throw std::out_of_range("Row index out of range");
    }
    if (col >= headers_.size()) {
        throw std::out_of_range("Column index out of range");
    }
}

void FastCSVContainer::rebuildColumnNameMap() {
    columnNameToIndex_.clear();
    for (int i = 0; i < headers_.size(); ++i) {
        columnNameToIndex_[headers_[i]] = static_cast<ColumnIndex>(i);
    }
}

} // namespace langscore