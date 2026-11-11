#include "FastCSVContainer.h"
#include "../../csv.hpp"
#include <algorithm>
#include <stdexcept>
#include <QFile>
#include <QSaveFile>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QRegularExpression>
#include <QProcess>
#include <QStandardPaths>

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
        data_.emplace_back(csvData[i]);
        
        // 列数が足りない場合は空文字で埋める
        if (data_.back().size() < headers_.size()) {
            data_.back().resize(headers_.size(), "");
        }
    }

    indexBuilt_ = false;
    return true;
}

namespace
{
    bool HasGitConflictMarkers(const QByteArray& contents)
    {
        //ほとんどのファイルには競合マーカーが無いため、行に分ける前に除外する
        if(contents.contains("<<<<<<<") == false) { return false; }
        //競合マーカーの行は必ず取り除かれるため、行数が減っていれば競合マーカーがある
        const auto lines = QString::fromUtf8(contents).split('\n');
        return pickGitConflictSide(lines, GitConflictSide::Ours).size() != lines.size();
    }

    //Git は競合中、base・ローカル側・リモート側の内容をステージ 1・2・3 として保持している
    //翻訳文に「=======」のみの行などがあると競合マーカーから両側を正しく分けられないため、取得できる場合はこちらを使う
    //git が無い場合や、Git で競合しているファイルでない場合は取得できない
    std::optional<QByteArray> ReadGitConflictStage(const QString& filePath, int stage)
    {
        const auto git = QStandardPaths::findExecutable("git");
        if(git.isEmpty()) { return std::nullopt; }
        const QFileInfo info(filePath);
        QProcess process;
        process.setWorkingDirectory(info.absolutePath());
        process.start(git, {"show", QString(":%1:./%2").arg(stage).arg(info.fileName())});
        if(process.waitForFinished(5000) == false)
        {
            process.kill();
            process.waitForFinished();
            return std::nullopt;
        }
        if(process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) { return std::nullopt; }
        return process.readAllStandardOutput();
    }
}

bool FastCSVContainer::loadFromFile(const CellData& filePath) {
    QFile file(filePath);
    if(file.open(QIODevice::ReadOnly) == false) { return false; }
    const auto contents = file.readAll();
    if(HasGitConflictMarkers(contents))
    {
        const auto ours = ReadGitConflictStage(filePath, 2);
        if(ours.has_value()) { return loadFromFileContents(ours.value(), GitConflictSide::Ours); }
    }
    return loadFromFileContents(contents, GitConflictSide::Ours);
}

bool FastCSVContainer::loadGitConflictSidesFromFile(const CellData& filePath, GitConflictSides& sides) {
    //2回に分けて読むと、その間の Git による書き換えや他のアプリによるロックで、両側が別時点の内容になる
    QFile file(filePath);
    if(file.open(QIODevice::ReadOnly) == false) { return false; }
    const auto contents = file.readAll();
    sides.hasConflictMarkers = HasGitConflictMarkers(contents);
    sides.base.reset();
    if(sides.hasConflictMarkers)
    {
        const auto ours = ReadGitConflictStage(filePath, 2);
        const auto theirs = ReadGitConflictStage(filePath, 3);
        if(ours.has_value() && theirs.has_value())
        {
            sides.ours.loadFromFileContents(ours.value(), GitConflictSide::Ours);
            sides.theirs.loadFromFileContents(theirs.value(), GitConflictSide::Ours);
            //ローカルとリモートで別々に追加されたファイルには base が無い
            const auto base = ReadGitConflictStage(filePath, 1);
            if(base.has_value()) { sides.base.emplace().loadFromFileContents(base.value(), GitConflictSide::Ours); }
            return true;
        }
    }
    //競合マーカーが無い場合、ローカル側は使わないため読み込まない
    if(sides.hasConflictMarkers) { sides.ours.loadFromFileContents(contents, GitConflictSide::Ours); }
    sides.theirs.loadFromFileContents(contents, GitConflictSide::Theirs);
    return true;
}

bool FastCSVContainer::loadFromFileContents(const QByteArray& contents, GitConflictSide side) {
    //CSV ファイルの値はエスケープされているため、ここで元に戻す
    //loadFromCsvData は JSON 由来などのエスケープされていない値も受け取るため、そちらでは戻さない
    //セル内の改行は、以前の保存で \r\r\n になったものも含め、アプリ内では \n に統一する
    //ゲームに合わせた改行への変換は、書き出し時に行う
    static const QRegularExpression lineBreak("\\r+\\n");
    auto csvData = langscore::parseCsv(contents, side);
    for (size_t i = 1; i < csvData.size(); ++i) {
        for (auto& field : csvData[i]) {
            field = unescapeCSVField(field);
            field.replace(lineBreak, "\n");
        }
    }
    return loadFromCsvData(csvData);
}

bool FastCSVContainer::saveToFile(const CellData& filePath) const
{
    // データが空の場合はfalseを返す
    if(isEmpty()) {
        return false;
    }

    //ディレクトリがなければ作成
    QDir dir = QFileInfo(filePath).absoluteDir();
    if(dir.exists() == false) {
        dir.mkpath(".");
    }

    //一時ファイルに書いてから置き換える。途中で失敗しても元のファイルは残る
    //Text モードではセル内の \r\n が \r\r\n に変換されるため、バイナリで書く
    QSaveFile file(filePath);
    if(file.open(QIODevice::WriteOnly) == false) {
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

    //バッファを書き出してから確認する。失敗した場合は commit せず、元のファイルを残す
    stream.flush();
    if(stream.status() != QTextStream::Ok) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

FastCSVContainer::CellData FastCSVContainer::unescapeCSVField(const CellData& field) const
{
    // 空文字またはダブルクォートで囲まれていない場合はそのまま返す
    if(field.isEmpty() || field.startsWith('"') == false || field.endsWith('"') == false) {
        return field;
    }

    // 長さが2未満の場合は空文字を返す
    if(field.length() < 2) {
        return "";
    }

    // 前後のダブルクォートを除去
    CellData unescaped = field.mid(1, field.length() - 2);

    // エスケープされたダブルクォート "" を " に戻す
    unescaped.replace("\"\"", "\"");

    return unescaped;
}

FastCSVContainer::CellData FastCSVContainer::escapeCSVField(const CellData& field) const
{
    // フィールドにカンマ、ダブルクォート、改行が含まれている場合はダブルクォートで囲む
    if(field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        CellData escaped = field;
        // ダブルクォートを "" にエスケープする
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
    indexBuilt_ = false; // インデックスの再構築が必要
}

void FastCSVContainer::removeRow(RowIndex index) {
    if (index >= data_.size()) {
        return;
    }
    
    data_.erase(data_.begin() + index);
    indexBuilt_ = false; // インデックスの再構築が必要
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