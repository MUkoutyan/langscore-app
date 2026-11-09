#include "CSVEditorTableModel.h"
#include "EditorTableDefines.h"
#include "../../csv.hpp"
#include "../../settings.h"
#include "../../utility.hpp"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QColor>
#include <QApplication>
#include <QStyle>
#include "../ValidationIcon.h"
#include <fstream>
#include "service/LanguageNames.h"

namespace langscore {

namespace
{
    //CSV を読むと改行 \r\n が \n になるため、改行をそろえてから比較する
    QString ToKey(QString text) { return text.replace("\r\n", "\n"); }

    bool IsTranslationColumn(const QString& header) { return header != "original" && header != "type"; }

    //改行をそろえた原文 → 行
    QHash<QString, size_t> RowsOfOriginal(const FastCSVContainer& container)
    {
        QHash<QString, size_t> result;
        if(container.hasColumn("original") == false) { return result; }
        const auto originalColumn = container.getColumnIndex("original");
        for(size_t row = 0; row < container.rowCount(); ++row) {
            result.insert(ToKey(container.at(row, originalColumn)), row);
        }
        return result;
    }
}

CSVEditorTableModel::CSVEditorTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

CSVEditorTableModel::CSVEditorTableModel(const QString& path, QObject* parent)
    : QAbstractTableModel(parent)
{
    loadFromEditCSVFile(path);
}

bool CSVEditorTableModel::loadFromEditCSVFile(const QString& csv_path) 
{
    beginResetModel();
    bool success = csvContainer.loadFromFile(csv_path);
    forgetSavedValues();
    rememberFileContents(csvContainer);
    _rowsLeftAtLoad.clear();
    _changedByOthers.clear();
    _conflictCells.clear();
    endResetModel();
    emit markStateChanged();
    if(success) {
        currentShowFileName = langscore::getFileNameWithoutExtension(csv_path);
    }
    return success;
}


bool CSVEditorTableModel::loadFromJsonFile(const QString& filePath)
{
    QFile file(filePath);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << filePath;
        return false;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << filePath;
        return false;
    }

    QJsonArray jsonArray = jsonDoc.array();
    if(jsonArray.isEmpty()) {
        qWarning() << "JSON array is empty in file:" << filePath;
        return false;
    }

    currentShowFileName = getFileNameWithoutExtension(filePath);

    beginResetModel();

    // コンテナをクリア
    csvContainer.clear();

    // ヘッダーを設定
    std::vector<std::vector<QString>> csvData;
    csvData.push_back({"original", "type"}); // ヘッダー行

    // データ行を処理
    int wordCount_ = 0;
    for(int r = 0; r < jsonArray.size(); ++r) {
        QJsonObject item = jsonArray.at(r).toObject();
        QString originalText = item["original"].toString();
        wordCount_ += langscore::wordCountUTF8(originalText);

        QJsonArray typeArray = item["type"].toArray();
        QStringList typeList;
        for(const QJsonValue& typeValue : typeArray) {
            typeList.append(typeValue.toString());
        }
        QString typeText = typeList.join(", ");

        csvData.push_back({originalText, typeText});
    }

    // FastCSVContainerにデータを読み込み
    bool success = csvContainer.loadFromCsvData(csvData);

    forgetSavedValues();
    //まだ editing のCSVを読んでいない。読む場合は mergeTranslationsFromCsv で覚える
    rememberFileContents({});
    _rowsLeftAtLoad.clear();
    _changedByOthers.clear();
    _conflictCells.clear();
    endResetModel();
    emit markStateChanged();
    return success;
}

//共有フォルダなどで、読み込んだ後に他の人が同じファイルを保存していた場合は、
//その人の変更を取り込んでから保存する。両方が変えたセルは自分の訳で保存し、mergeConflicts に残す
bool CSVEditorTableModel::saveToFile(const QString& path)
{
    _mergeConflicts.clear();
    FastCSVContainer file;
    const bool hasFile = QFile::exists(path) && file.loadFromFile(path) && file.hasColumn("original");
    if(hasFile) { _mergeConflicts = takeChangesFromFile(file); }

    const auto output = hasFile ? withUnknownContents(file) : csvContainer;
    const bool success = output.saveToFile(path);
    if(success)
    {
        rememberFileContents(output);
        _savedValues.clear();
        _modifiedWithoutRecord = false;
        //衝突したセルは自分の訳で保存したため、ファイルとの衝突ではなくなる（mergeConflicts で聞く）
        _conflictCells.clear();
        setModified(false);
        emit markStateChanged();
    }
    return success;
}

bool CSVEditorTableModel::needsSave() const
{
    if(_modified) { return true; }
    for(const auto& header : csvContainer.headers()) {
        if(_fileHeaders.contains(header) == false) { return true; }
    }
    return false;
}

void CSVEditorTableModel::rememberFileContents(const FastCSVContainer& file)
{
    _fileHeaders = file.headers();
    _fileValues.clear();
    if(file.hasColumn("original") == false) { return; }

    const auto originalColumn = file.getColumnIndex("original");
    for(size_t row = 0; row < file.rowCount(); ++row)
    {
        const auto original = ToKey(file.at(row, originalColumn));
        for(size_t column = 0; column < file.columnCount(); ++column)
        {
            const auto& header = file.headerAt(column);
            if(IsTranslationColumn(header) == false) { continue; }
            _fileValues.insert({original, header}, file.at(row, column));
        }
    }
}

std::vector<CSVEditorTableModel::MergeConflict> CSVEditorTableModel::syncWithFile(const QString& path)
{
    FastCSVContainer file;
    if(QFile::exists(path) == false || file.loadFromFile(path) == false || file.hasColumn("original") == false) { return {}; }
    _fileHeaders = file.headers();
    return takeChangesFromFile(file);
}

void CSVEditorTableModel::resolveConflict(const MergeConflict& conflict, bool useTheirs)
{
    if(conflict.row < 0 || rowCount() <= conflict.row) { return; }
    if(conflict.column < 0 || columnCount() <= conflict.column) { return; }

    if(useTheirs) { takeTheirs(conflict.row, conflict.column, conflict.theirs); }
    //他の人の訳を見たものとして覚える。自分の訳にした場合は、自分の変更として次の保存で書く
    rememberFileValue(conflict.row, conflict.column, conflict.theirs);
    _conflictCells.remove({conflict.row, conflict.column});
    emit markStateChanged();
}

void CSVEditorTableModel::rememberFileValue(int row, int column, const QString& value)
{
    const auto cellRow = static_cast<size_t>(row);
    const auto cellColumn = static_cast<size_t>(column);
    const auto original = ToKey(csvContainer.at(cellRow, QString("original")));
    _fileValues.insert({original, csvContainer.headerAt(cellColumn)}, value);

    //_savedValues はファイルの値。ファイルと同じになったセルは、変更なしとして扱う
    const auto key = std::pair(row, column);
    if(ToKey(csvContainer.at(cellRow, cellColumn)) == ToKey(value)) {
        _savedValues.remove(key);
    }
    else {
        _savedValues.insert(key, value);
    }
    setModified(_modifiedWithoutRecord || _savedValues.isEmpty() == false);
}

void CSVEditorTableModel::takeTheirs(int row, int column, const QString& theirs)
{
    const auto cellRow = static_cast<size_t>(row);
    const auto cellColumn = static_cast<size_t>(column);
    const auto before = csvContainer.at(cellRow, cellColumn);
    csvContainer.setValue(cellRow, cellColumn, theirs);
    _editedSinceValidation.insert({row, column});

    //すでに印があるセルは、最初の訳を変わる前の訳として残す
    const auto key = cellKey(row, column);
    if(_changedByOthers.contains(key) == false) { _changedByOthers.insert(key, before); }
    emit dataChanged(index(row, column), index(row, column));
    emit changedByOthers(csvContainer.at(cellRow, QString("original")), key.second, before, theirs);
}

std::pair<QString, QString> CSVEditorTableModel::cellKey(int row, int column) const
{
    const auto cellRow = static_cast<size_t>(row);
    const auto cellColumn = static_cast<size_t>(column);
    if(csvContainer.hasColumn("original") == false) { return {}; }
    return {ToKey(csvContainer.at(cellRow, QString("original"))), csvContainer.headerAt(cellColumn)};
}

void CSVEditorTableModel::clearChangedByOthers()
{
    if(_changedByOthers.isEmpty()) { return; }
    _changedByOthers.clear();
    emit markStateChanged();
    if(rowCount() == 0 || columnCount() == 0) { return; }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

std::vector<CSVEditorTableModel::MergeConflict> CSVEditorTableModel::takeChangesFromFile(const FastCSVContainer& file)
{
    std::vector<MergeConflict> conflicts;
    if(csvContainer.hasColumn("original") == false) { return conflicts; }
    const auto originalColumn = csvContainer.getColumnIndex("original");
    const auto fileRowOfOriginal = RowsOfOriginal(file);

    for(size_t row = 0; row < csvContainer.rowCount(); ++row)
    {
        const auto original = ToKey(csvContainer.at(row, originalColumn));
        const auto fileRow = fileRowOfOriginal.constFind(original);
        //ファイルに無い行は、他の人が変えていないものとして扱う
        if(fileRow == fileRowOfOriginal.cend()) { continue; }

        for(size_t column = 0; column < csvContainer.columnCount(); ++column)
        {
            const auto& header = csvContainer.headerAt(column);
            if(IsTranslationColumn(header) == false || file.hasColumn(header) == false) { continue; }

            const auto& theirs = file.at(fileRow.value(), header);
            const auto mine = csvContainer.at(row, column);
            const auto base = _fileValues.value({original, header});
            //他の人が変えていない
            if(ToKey(theirs) == ToKey(base)) { continue; }

            const auto cellRow = static_cast<int>(row);
            const auto cellColumn = static_cast<int>(column);
            const bool changedByMe = ToKey(mine) != ToKey(base);
            if(changedByMe && ToKey(theirs) != ToKey(mine))
            {
                //衝突したセルは、決めるまでファイルの値を覚え直さない（読み直すたびに衝突として返す）
                conflicts.push_back({cellRow, cellColumn, mine, theirs});
                continue;
            }
            //他の人だけが変えたセルは、その人の訳にする
            if(changedByMe == false) { takeTheirs(cellRow, cellColumn, theirs); }
            rememberFileValue(cellRow, cellColumn, theirs);
        }
    }
    //決めていない衝突は、読み直すたびにすべて返るため、その結果に置き換える
    _conflictCells.clear();
    for(const auto& conflict : conflicts) {
        _conflictCells.insert({conflict.row, conflict.column});
    }
    emit markStateChanged();
    return conflicts;
}

FastCSVContainer CSVEditorTableModel::withUnknownContents(const FastCSVContainer& file) const
{
    auto output = csvContainer;
    if(output.hasColumn("original") == false) { return output; }

    //ファイルにだけある言語の列（他の人が有効にした言語など）を、type 列の前に足す
    QStringList addedColumns;
    for(const auto& header : file.headers())
    {
        if(IsTranslationColumn(header) == false || output.hasColumn(header)) { continue; }
        const auto typeColumn = output.headers().indexOf("type");
        const auto insertIndex = typeColumn < 0 ? output.columnCount() : static_cast<size_t>(typeColumn);
        output.insertColumn(insertIndex, header, "");
        addedColumns.append(header);
    }

    const auto fileRowOfOriginal = RowsOfOriginal(file);
    const auto originalColumn = output.getColumnIndex("original");
    QSet<QString> originals;
    for(size_t row = 0; row < output.rowCount(); ++row)
    {
        const auto original = ToKey(output.at(row, originalColumn));
        originals.insert(original);
        const auto fileRow = fileRowOfOriginal.constFind(original);
        if(fileRow == fileRowOfOriginal.cend()) { continue; }
        for(const auto& header : addedColumns) {
            output.setValue(row, header, file.at(fileRow.value(), header));
        }
    }

    //表に無い行（他の人が解析し直して増えた行など）を残す。読み込んだときに古い翻訳へ移した行は消す
    for(size_t fileRow = 0; fileRow < file.rowCount(); ++fileRow)
    {
        const auto original = ToKey(file.at(fileRow, QString("original")));
        if(originals.contains(original) || _rowsLeftAtLoad.contains(original)) { continue; }
        std::vector<QString> rowData;
        for(const auto& header : output.headers())
        {
            QString value;
            if(file.hasColumn(header)) { value = file.at(fileRow, header); }
            rowData.push_back(value);
        }
        output.addRow(std::move(rowData));
    }
    return output;
}

void CSVEditorTableModel::setModified(bool modified)
{
    if(_modified == modified) { return; }
    _modified = modified;
    emit modifiedChanged(modified);
    emit markStateChanged();
}

void CSVEditorTableModel::forgetSavedValues()
{
    if(_savedValues.isEmpty() == false) { _modifiedWithoutRecord = true; }
    _savedValues.clear();
}

void CSVEditorTableModel::clearEditedSinceValidation()
{
    if(_editedSinceValidation.isEmpty()) { return; }
    _editedSinceValidation.clear();
    if(rowCount() == 0 || columnCount() == 0) { return; }
    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1), {Qt::BackgroundRole, Qt::DecorationRole, Qt::ToolTipRole});
}

const std::vector<ValidationErrorInfo>* CSVEditorTableModel::indexedFileErrors() const
{
    if(this->_runtimeData == nullptr) { return nullptr; }
    const auto& errors = this->_runtimeData->errors;
    const auto found = errors.find(currentShowFileName);
    if(found == errors.end() || found->second.empty()) { return nullptr; }

    //検証中は結果が増えていくため、件数が変わったときも作り直す
    const auto& list = found->second;
    if(_errorIndex.revision != _runtimeData->errorsRevision || _errorIndex.size != list.size())
    {
        _errorIndex = ErrorIndex{_runtimeData->errorsRevision, list.size()};
        for(size_t i = 0; i < list.size(); ++i)
        {
            const auto& error = list[i];
            const auto languages = error.language.split(' ', Qt::SkipEmptyParts);
            if(error.row == 0 && languages.isEmpty()) {
                _errorIndex.fileLevel.push_back(i);
                continue;
            }
            //error.row はヘッダーを 0 行目とした CSV の行
            for(const auto& lang : languages) {
                _errorIndex.cells[{static_cast<int>(error.row - 1), lang}].push_back(i);
            }
        }
    }
    return &list;
}

//セルに対応する検証結果を返す（原文・種類の列は対象外）
std::vector<const ValidationErrorInfo*> CSVEditorTableModel::cellErrors(int row, int col) const
{
    std::vector<const ValidationErrorInfo*> result;
    if(col < 0 || static_cast<size_t>(col) >= csvContainer.columnCount()) { return result; }
    const auto& headerText = csvContainer.headers()[col];
    if(headerText == "type" || headerText == "original") { return result; }

    const auto* list = indexedFileErrors();
    if(list == nullptr) { return result; }
    const auto found = _errorIndex.cells.constFind({row, headerText});
    if(found == _errorIndex.cells.cend()) { return result; }
    for(const auto i : found.value()) {
        if(_runtimeData->isErrorShown((*list)[i])) { result.push_back(&(*list)[i]); }
    }
    return result;
}

std::vector<const ValidationErrorInfo*> CSVEditorTableModel::fileErrors() const
{
    std::vector<const ValidationErrorInfo*> result;
    const auto* list = indexedFileErrors();
    if(list == nullptr) { return result; }
    for(const auto i : _errorIndex.fileLevel) {
        if(_runtimeData->isErrorShown((*list)[i])) { result.push_back(&(*list)[i]); }
    }
    return result;
}

//すべての言語列が埋まっている行の数と、全体の行数を返す（無効にした言語は数えない）
std::pair<int, int> CSVEditorTableModel::translatedRowCount() const
{
    std::vector<size_t> languageColumns;
    if(_settings)
    {
        for(const auto& lang : _settings->languages)
        {
            if(lang.enable == false) { continue; }
            const auto column = csvContainer.headers().indexOf(lang.languageName);
            if(0 <= column) {
                languageColumns.push_back(static_cast<size_t>(column));
            }
        }
    }
    const auto totalRows = static_cast<int>(csvContainer.rowCount());
    if(languageColumns.empty()) { return {0, totalRows}; }

    int translatedRows = 0;
    for(size_t row = 0; row < csvContainer.rowCount(); ++row)
    {
        const bool filledAll = std::ranges::all_of(languageColumns, [this, row](size_t column) {
            return csvContainer[row][column].isEmpty() == false;
        });
        if(filledAll) {
            translatedRows++;
        }
    }
    return {translatedRows, totalRows};
}

int CSVEditorTableModel::rowCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.rowCount());
}

int CSVEditorTableModel::columnCount(const QModelIndex&) const {
    return static_cast<int>(csvContainer.columnCount());
}

QVariant CSVEditorTableModel::data(const QModelIndex& index, int role) const 
{
    if(index.isValid() == false) {
        return QVariant();
    }

    int row = index.row();
    int col = index.column();

    if(role == Qt::FontRole) {
        if(_settings && this->useLanguageFont) {
            QString header = headerData(col, Qt::Horizontal, Qt::UserRole).toString();
            for(const auto& lang : _settings->languages) {
                if(lang.languageName == header) {
                    auto font = lang.font.fontData;
                    font.setPixelSize(12);
                    return font;
                }
            }
        }
        return QVariant();
    }
    else if((role == Qt::BackgroundRole || role == Qt::DecorationRole) && _changedByOthers.isEmpty() == false && _changedByOthers.contains(cellKey(row, col)))
    {
        //他の人の訳にしたセル。色だけでは見分けにくいため、アイコンも付ける
        if(role == Qt::DecorationRole) { return QApplication::style()->standardIcon(QStyle::SP_BrowserReload); }
        return QColor(0, 120, 215, 64);
    }
    else if(role == Qt::BackgroundRole || role == Qt::DecorationRole)
    {
        //検証の後に編集したセルは、結果が古いため色を付けない
        if(_editedSinceValidation.contains({row, col})) { return QVariant(); }
        const auto cellErrorList = cellErrors(row, col);
        const bool isError = std::ranges::any_of(cellErrorList, [](const auto* x) { return x->type == ValidationErrorInfo::Error; });
        const bool isWarning = std::ranges::any_of(cellErrorList, [](const auto* x) { return x->type == ValidationErrorInfo::Warning; });
        //色だけでは見分けにくいため、アイコンも付ける
        if(role == Qt::DecorationRole)
        {
            if(isError) { return ValidationIcon(ValidationErrorInfo::Error); }
            if(isWarning) { return ValidationIcon(ValidationErrorInfo::Warning); }
            return QVariant();
        }
        if(isError) { return QColor(236, 11, 0, 51); }
        if(isWarning) { return QColor(240, 227, 0, 51); }
        return QVariant();
    }
    else if(role == Qt::ToolTipRole)
    {
        QStringList lines;
        if(const auto before = _changedByOthers.constFind(cellKey(row, col)); before != _changedByOthers.cend()) {
            //他の人が変更しました（変更前: %1）
            lines.append(tr("Changed by someone else (before: %1)").arg(before.value().isEmpty() ? tr("(empty)") : before.value()));
        }
        if(_editedSinceValidation.contains({row, col}) == false)
        {
            for(const auto* error : cellErrors(row, col))
            {
                if(error->summary == ValidationErrorInfo::IncludeCR) { continue; }
                const auto prefix = (error->type == ValidationErrorInfo::Error) ? tr("[Error] ") : tr("[Warning] ");
                lines.append(prefix + error->getErrorText());
            }
        }
        //翻訳文の長さを原文と比べられるようにする
        const auto originalColumn = csvContainer.headers().indexOf("original");
        if(0 <= originalColumn && headerData(col, Qt::Horizontal, DataType::IsLanguageColumn).toBool())
        {
            QString translated;
            QString original;
            csvContainer.getValue(static_cast<size_t>(row), static_cast<size_t>(col), translated);
            csvContainer.getValue(static_cast<size_t>(row), static_cast<size_t>(originalColumn), original);
            if(translated.isEmpty() == false) {
                lines.append(tr("Characters: %1 (Original: %2)").arg(translated.length()).arg(original.length()));
            }
        }
        if(lines.isEmpty()) { return QVariant(); }
        return lines.join("\n");
    }
    else if(role == Qt::UserRole)
    {
        const auto cellErrorList = cellErrors(row, col);
        if(cellErrorList.empty() == false) {
            return cellErrorList.front()->id;
        }
        return QVariant();
    }

    if(role != Qt::DisplayRole && role != Qt::EditRole) {
        return QVariant();
    }

    QString value;
    if(csvContainer.getValue(static_cast<size_t>(row), static_cast<size_t>(col), value)) 
    {
        FastCSVContainer::CellData headerData;
        if(col >= 0 && col < static_cast<int>(csvContainer.columnCount())) {
            headerData = csvContainer.headerAt(static_cast<size_t>(col));
        }
        
        if(headerData == "type") 
        {
            const auto toDisplayName = [](const QString& type) -> QString {
                if(type == "name") { return tr("name"); }
                if(type == "description") { return tr("description"); }
                if(type == "messageWithIcon") { return tr("messageWithIcon"); }
                if(type == "battleName") { return tr("battleName"); }
                if(type == "battleMessage") { return tr("battleMessage"); }
                if(type == "message") { return tr("message"); }
                if(type == "note") { return tr("note"); }
                if(type == "other") { return tr("other"); }
                return type;
            };
            auto types = value.split(", ");
            std::ranges::transform(types, types.begin(), toDisplayName);
            return types.join(", ");
        }
        else 
        {
            return value;
        }
    }

    return QVariant();
}

bool CSVEditorTableModel::setData(const QModelIndex& index, const QVariant& value, int role) 
{
    if(role != Qt::EditRole || !index.isValid()) {
        return false;
    }
    
    //original・type 列は編集不可。貼り付けや削除など、どの経路から来ても書き換えない
    if((flags(index) & Qt::ItemIsEditable) == 0) {
        return false;
    }

    int row = index.row();
    int col = index.column();
    const auto newValue = value.toString();
    QString oldValue;
    csvContainer.getValue(static_cast<size_t>(row), static_cast<size_t>(col), oldValue);

    if(csvContainer.setValue(static_cast<size_t>(row), static_cast<size_t>(col), newValue))
    {
        recordEdit(row, col, oldValue, newValue);
        emit dataChanged(index, index);
        return true;
    }

    return false;
}

void CSVEditorTableModel::recordEdit(int row, int col, const QString& oldValue, const QString& newValue)
{
    _editedSinceValidation.insert({row, col});
    //自分で編集したセルは、他の人の訳の印を消す
    if(_changedByOthers.isEmpty() == false && _changedByOthers.remove(cellKey(row, col))) { emit markStateChanged(); }
    //保存したときの値に戻ったセルは、変更なしとして扱う
    const auto key = std::pair(row, col);
    const auto saved = _savedValues.constFind(key);
    if(saved == _savedValues.cend()) {
        if(oldValue != newValue) { _savedValues.insert(key, oldValue); }
    }
    else if(saved.value() == newValue) {
        _savedValues.erase(saved);
    }
    setModified(_modifiedWithoutRecord || _savedValues.isEmpty() == false);
}

Qt::ItemFlags CSVEditorTableModel::flags(const QModelIndex& index) const 
{
    if(!index.isValid()) {
        return Qt::NoItemFlags;
    }
    
    // ヘッダーの内容を参照して、originalとtype以外の列は編集可能にする
    int col = index.column();
    
    FastCSVContainer::CellData headerData;
    if(col >= 0 && col < static_cast<int>(csvContainer.columnCount())) {
        headerData = csvContainer.headerAt(static_cast<size_t>(col));
    }
    if(headerData == "original" || headerData == "type") {
        return Qt::ItemIsSelectable | Qt::ItemIsEnabled; // 編集不可
    }
    
    return Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled;
}

QVariant CSVEditorTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal) {
        //行の番号は1から。検証結果の行（CSVのヘッダーを0行目とした行）と同じ番号になる
        return role == Qt::DisplayRole ? QVariant(section + 1) : QVariant();
    }
    //headerAt は範囲外で例外を投げるため、先に範囲を確認する
    if(section < 0 || section >= static_cast<int>(csvContainer.columnCount())) {
        return role == DataType::IsLanguageColumn ? QVariant(false) : QVariant();
    }

    if(role == Qt::DisplayRole)
    {
        {
            auto name = csvContainer.headerAt(static_cast<size_t>(section));
            if(name == "original") {
                return tr("Original");
            }
            else if(name == "type") {
                return tr("Type");
            }
            else 
            {
                QString localeName = name;
                QString display = langscore::languageDisplayName(localeName);
                if(display.isEmpty() == false) {
                    return display + "(" + name + ")";
                }

                QLocale locale(localeName);
                return locale.nativeLanguageName() + "(" + name + ")"; // 他の列名はそのまま返す
            }
        }
    }
    else if(role == DataType::FilterText) {
        return csvContainer.headerAt(static_cast<size_t>(section));
    }
    else if(role == DataType::IsLanguageColumn) {
        //MainCSVTable は setSettings より前に proxy へ setSourceModel するため、未設定の場合がある
        if(_settings == nullptr) {
            return false;
        }
        auto name = csvContainer.headerAt(static_cast<size_t>(section));
        if(std::ranges::find(_settings->languages, name, &settings::Language::languageName) != _settings->languages.cend()) {
            return true;
        }
        return false;
    }

    return QVariant();
}

size_t CSVEditorTableModel::rowCountRaw() const {
    return csvContainer.rowCount();
}

size_t CSVEditorTableModel::colCountRaw() const {
    return csvContainer.columnCount();
}

std::optional<QString> CSVEditorTableModel::getCell(size_t row, size_t col) const 
{
    if(row >= csvContainer.rowCount() || col >= csvContainer.columnCount()) { return std::nullopt; }
    return csvContainer[row][col];
}

bool CSVEditorTableModel::setCell(size_t row, size_t col, const QString& value) 
{
    if(row >= csvContainer.rowCount()) { return false; }

    const auto oldValue = csvContainer[row][col];
    csvContainer[row][col] = value;
    recordEdit(static_cast<int>(row), static_cast<int>(col), oldValue, value);
    QModelIndex idx = index(static_cast<int>(row), static_cast<int>(col));
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    return true;
}


void CSVEditorTableModel::insertColumn(size_t index, const QString& columnName)
{
    beginResetModel();
    csvContainer.insertColumn(index, columnName, "");
    forgetSavedValues();
    endResetModel();
}

void CSVEditorTableModel::removeColumn(int column)
{
    if(column < 0 || column >= static_cast<int>(csvContainer.columnCount())) {
        return;
    }

    beginResetModel();
    csvContainer.removeColumn(static_cast<size_t>(column));
    forgetSavedValues();
    endResetModel();
}


const std::vector<std::vector<QString>>& CSVEditorTableModel::dataRaw() const {
    return csvContainer.dataRaw();
}


//editing に保存したCSVの翻訳を、原文が一致する行へ戻す。
//今の表に無い言語の列（無効にした言語）は、翻訳を失わないよう列ごと足す
bool CSVEditorTableModel::mergeTranslationsFromCsv(const QString& csvPath)
{
    FastCSVContainer saved;
    if(saved.loadFromFile(csvPath) == false) { return false; }

    const auto& savedHeaders = saved.headers();
    const auto savedOriginalColumn = savedHeaders.indexOf("original");
    if(savedOriginalColumn < 0) { return false; }

    //原文から、保存したCSVの行を参照できるようにする
    //CSV を読むと原文の改行 \r\n が \n になるため、改行をそろえてから比較する
    const auto toKey = [](QString text) { return text.replace("\r\n", "\n"); };
    QHash<QString, size_t> savedRowOfOriginal;
    for(size_t row = 0; row < saved.rowCount(); ++row) {
        savedRowOfOriginal.insert(toKey(saved[row][static_cast<size_t>(savedOriginalColumn)]), row);
    }

    const auto originalColumn = static_cast<size_t>(csvContainer.headers().indexOf("original"));

    //保存したCSVにあり、今の表に無い行（古い翻訳へ移した行）は、次の保存で消す
    QSet<QString> originals;
    for(size_t row = 0; row < csvContainer.rowCount(); ++row) {
        originals.insert(toKey(csvContainer[row][originalColumn]));
    }
    _rowsLeftAtLoad.clear();
    for(auto it = savedRowOfOriginal.cbegin(); it != savedRowOfOriginal.cend(); ++it) {
        if(originals.contains(it.key()) == false) { _rowsLeftAtLoad.insert(it.key()); }
    }

    beginResetModel();
    for(qsizetype savedColumn = 0; savedColumn < savedHeaders.size(); ++savedColumn)
    {
        const auto& language = savedHeaders[savedColumn];
        if(language == "original" || language == "type") { continue; }

        //無効にした言語の列は今の表に無いので、type 列の前に足す
        if(csvContainer.headers().contains(language) == false)
        {
            const auto typeColumn = csvContainer.headers().indexOf("type");
            const auto insertIndex = typeColumn < 0 ? csvContainer.columnCount() : static_cast<size_t>(typeColumn);
            csvContainer.insertColumn(insertIndex, language, "");
        }
        const auto column = static_cast<size_t>(csvContainer.headers().indexOf(language));

        for(size_t row = 0; row < csvContainer.rowCount(); ++row)
        {
            const auto savedRow = savedRowOfOriginal.constFind(toKey(csvContainer[row][originalColumn]));
            if(savedRow == savedRowOfOriginal.cend()) { continue; }
            csvContainer[row][column] = saved[savedRow.value()][static_cast<size_t>(savedColumn)];
        }
    }
    forgetSavedValues();
    rememberFileContents(saved);
    endResetModel();
    return true;
}


void CSVEditorTableModel::setUseLanguageFont(bool use) {
    this->useLanguageFont = use;
}

void CSVEditorTableModel::setSettings(std::shared_ptr<settings> setting)
{
    this->_settings = std::move(setting);
}

void CSVEditorTableModel::setRuntimeData(std::shared_ptr<ComponentBase::RuntimeData> setting)
{
    this->_runtimeData = std::move(setting);
}

} // namespace langscore