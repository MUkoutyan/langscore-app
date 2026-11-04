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
#include "../ValidationIcon.h"
#include <fstream>
#include "service/LanguageNames.h"

namespace langscore {

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
    endResetModel();
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

    endResetModel();
    return success;
}

bool CSVEditorTableModel::saveToFile(const QString& path)
{
    const bool success = csvContainer.saveToFile(path);
    if(success) {
        setModified(false);
    }
    return success;
}

void CSVEditorTableModel::setModified(bool modified)
{
    if(_modified == modified) { return; }
    _modified = modified;
    emit modifiedChanged(modified);
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

    if(csvContainer.setValue(static_cast<size_t>(row), static_cast<size_t>(col), value.toString()))
    {
        _editedSinceValidation.insert({row, col});
        setModified(true);
        emit dataChanged(index, index);
        return true;
    }

    return false;
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
        return QVariant();
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

    csvContainer[row][col] = value;
    QModelIndex idx = index(static_cast<int>(row), static_cast<int>(col));
    emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
    return true;
}


void CSVEditorTableModel::insertColumn(size_t index, const QString& columnName)
{
    beginResetModel();
    csvContainer.insertColumn(index, columnName, "");
    endResetModel();
}

void CSVEditorTableModel::removeColumn(int column)
{
    if(column < 0 || column >= static_cast<int>(csvContainer.columnCount())) {
        return;
    }

    beginResetModel();
    csvContainer.removeColumn(static_cast<size_t>(column));
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