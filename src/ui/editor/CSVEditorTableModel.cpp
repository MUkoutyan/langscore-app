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
        _originalData = csvContainer.dataRaw();
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

bool CSVEditorTableModel::saveToFile(const QString& path) const
{
    return csvContainer.saveToFile(path);
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
    else if(role == Qt::BackgroundRole)
    {
        if(this->_runtimeData == nullptr) {
            return QVariant();
        }
        const auto& errors = this->_runtimeData->errors;
        if(errors.empty()) {
            return QVariant();
        }

        if(errors.find(currentShowFileName) == errors.end()) {
            return QVariant();
        }
        const auto& headerText = csvContainer.headers()[col];
        if(headerText == "type" || headerText == "original") { return QVariant(); }

        const auto& errorList = errors.at(currentShowFileName);

        auto find_result = std::ranges::find_if(errorList, [this, row, headerText](const auto& x) {
            return x.row - 1 == row && x.language.contains(headerText);
        });
        if(find_result != errorList.end()) {
            if(find_result->type == ValidationErrorInfo::Error) {
                return QColor(236, 11, 0, 51);
            }
            else if(find_result->type == ValidationErrorInfo::Warning) {
                return QColor(240, 227, 0, 51);
            }
        }
    }
    else if(role == Qt::UserRole)
    {
        if(this->_runtimeData == nullptr) {
            return QVariant();
        }
        const auto& errors = this->_runtimeData->errors;
        if(errors.empty()) {
            return QVariant();
        }

        if(errors.find(currentShowFileName) == errors.end()) {
            return QVariant();
        }
        const auto& headerText = csvContainer.headers()[col];
        if(headerText == "type" || headerText == "original") { return QVariant(); }

        const auto& errorList = errors.at(currentShowFileName);

        auto find_result = std::ranges::find_if(errorList, [this, row, headerText](const auto& x) {
            return x.row - 1 == row && x.language.contains(headerText);
        });
        if(find_result != errorList.end()) {
            return find_result->id;
        }
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
            if(value == "name"){
                return tr("name");
            }
            else if(value == "description"){
                return tr("description");
            }
            else if(value == "messageWithIcon"){
                return tr("messageWithIcon");
            }
            else if(value == "battleName"){
                return tr("battleName");
            }
            else if(value == "battleMessage"){
                return tr("battleMessage");
            }
            else if(value == "message"){
                return tr("message");
            }
            else if(value == "note"){
                return tr("note");
            }
            else if(value == "other") {
                return tr("other");
            }
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

    int row = index.row();
    int col = index.column();

    if(csvContainer.setValue(static_cast<size_t>(row), static_cast<size_t>(col), value.toString())) {
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

    if(role == Qt::DisplayRole) 
    {
        if(section >= 0 && section < static_cast<int>(csvContainer.columnCount())) {
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

void CSVEditorTableModel::applySort(int column, int order)
{
    if(column < 0 || column >= static_cast<int>(csvContainer.columnCount())) {
        return;
    }

    if(order == 0) {
        // ソート解除
        if(!_originalData.empty()) {
            beginResetModel();
            csvContainer.loadFromCsvData(_originalData);
            endResetModel();
        }
        _sortedColumn = -1;
        _sortOrder = 0;
        return;
    }

    // Build vector of row indices
    std::vector<int> indices(static_cast<size_t>(csvContainer.rowCount()));
    std::ranges::iota(indices, 0);

    // Sort indices based on column value
    std::ranges::stable_sort(indices, [this, column, order](int a, int b) 
    {
        QString va, vb;
        csvContainer.getValue(static_cast<size_t>(a), static_cast<size_t>(column), va);
        csvContainer.getValue(static_cast<size_t>(b), static_cast<size_t>(column), vb);
        if(order == 1) {
            return va < vb;
        }
        return va > vb;
    });

    // Rebuild data in new order
    std::vector<std::vector<QString>> newData;
    newData.reserve(indices.size());
    for(int idx : indices) {
        newData.push_back(csvContainer.dataRaw()[static_cast<size_t>(idx)]);
    }

    beginResetModel();
    csvContainer.loadFromCsvData(newData);
    endResetModel();

    _sortedColumn = column;
    _sortOrder = order;
}

bool CSVEditorTableModel::isLanguageColumnHidden(const QString& language) const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.beginGroup("filters");
    QVariant v = settings.value(language.toLower(), QVariant());
    settings.endGroup();
    
    //設定値に保存されていればそれを返す
    if(v.isValid()) {
        return v.toBool();
    }

    //表示されていたらtrueを返す
    for(int col = 0; col < this->columnCount(); ++col) {
        QString headerText = this->headerData(col, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
        if(headerText == language.toLower()) {
            return true;
        }
    }
    return false;
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