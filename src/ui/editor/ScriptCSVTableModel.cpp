#include "ScriptCSVTableModel.h"
#include "CSVEditorTableModel.h"
#include "EditorTableDefines.h"

#include <QFile>
#include <QBrush>
#include <QLocale>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <ranges>
#include <algorithm>

#include "../../settings.h"
#include "../../utility.hpp"
#include "../ComponentBase.h"
#include "../editor/FastCSVContainer.h"
#include "service/LanguageNames.h"

using namespace langscore;

// ────────────────────────────────────────────────────────────
ScriptCSVTableModel::ScriptCSVTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
    this->setObjectName("ScriptCSVTableModel");
}

void ScriptCSVTableModel::setSettings(std::shared_ptr<settings> setting)
{
    _settings = std::move(setting);
}

void ScriptCSVTableModel::setRuntimeData(std::shared_ptr<ComponentBase::RuntimeData> setting)
{
    _runtimeData = std::move(setting);
}

// ────────────────────────────────────────────────────────────
void ScriptCSVTableModel::loadFromEditJSONWithSettings(const QString& editingDir)
{
    beginResetModel();
    _rows.clear();
    _languages.clear();
    _dirtyFiles.clear();
    _scriptNameToRows.clear();

    if(_settings == nullptr) {
        endResetModel();
        return;
    }

    // 有効言語コードを収集
    for(const auto& lang : _settings->languages) {
        if(lang.enable) {
            _languages.append(lang.languageName);
        }
    }

    auto scripts = _settings->getScriptFileList();

    // langscore 内部スクリプトを除外
    {
        auto rm = std::ranges::remove_if(scripts, [](const auto& d) {
            return settings::isLangscoreScript(d.scriptName);
        });
        scripts.erase(rm.begin(), rm.end());
    }

    const auto& scriptExt = GetScriptExtension(_settings->projectType);
    const auto jsonPath = editingDir + "/Scripts.lsjson";

    QMap<QString, QVector<QString>> origToTrans;
    if(QFile::exists(jsonPath))
    {
        QFile file(jsonPath);
        if(file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if(doc.isArray())
            {
                auto list = doc.array();
                for(const QJsonValue& val : list)
                {
                    const QJsonObject obj = val.toObject();
                    const QString orig = obj["original"].toString();
                    auto translates = obj["translates"].toObject();
                    QVector<QString> trans;
                    trans.reserve(_languages.size());
                    for(const auto& lang : _languages) 
                    {
                        if(translates.contains(lang)) {
                            trans.append(obj[lang].toString());
                        }
                    }
                    origToTrans.insert(orig, trans);
                }
            }
        }
    }

    for(const auto& scriptData : scripts)
    {
        // 表示名（拡張子なし）
        auto displayName = scriptData.scriptName;
        if(displayName.contains(scriptExt)) {
            displayName.chop(scriptExt.length());
        }

        for(const auto& line : scriptData.lines)
        {
            ScriptRowData rd;
            rd.include = true;
            rd.scriptIgnored = scriptData.ignore; // スクリプトレベルの無視フラグ
            rd.scriptDisplayName = scriptData.scriptName; // キャッシュキー兼表示名
            rd.scriptFileName = scriptData.fileName;
            rd.originalText = line.originalText;

            // TextPoint 文字列を構築
            if(std::holds_alternative<ScriptTextPosition::RowCol>(line.d)) {
                const auto& cell = std::get<ScriptTextPosition::RowCol>(line.d);
                if(cell.row == 0 && cell.col == 0) {
                    rd.textPoint = tr("Parameter");
                }
                else {
                    rd.textPoint = QString("%1:%2").arg(cell.row).arg(cell.col);
                }
            }
            else {
                const auto& cell = std::get<ScriptTextPosition::ScriptArg>(line.d);
                rd.textPoint = cell.valueName;
            }
            rd.position = line;

            // JSON から翻訳を取得
            rd.translations.resize(_languages.size(), "");
            const auto transIt = origToTrans.find(rd.originalText);
            if(transIt != origToTrans.end()) {
                rd.translations = transIt.value();
            }

            const int newRow = static_cast<int>(_rows.size());
            _rows.append(rd);
            _scriptNameToRows[rd.scriptDisplayName].push_back(newRow);
        }
    }

    endResetModel();
}

bool ScriptCSVTableModel::saveToFile(const QString& editingDir) const
{
    return saveToEditJsonFile(editingDir);
}

bool ScriptCSVTableModel::saveToEditJsonFile(const QString& editJsonFile) const
{
    if(_rows.isEmpty()) { return true; }

    // スクリプトファイル別にグループ化
    QMap<QString, QVector<const ScriptRowData*>> byScript;
    for(const auto& rd : _rows) {
        byScript[rd.scriptFileName].append(&rd);
    }

    QJsonArray jsonArray;
    bool success = true;
    for(auto it = byScript.constBegin(); it != byScript.constEnd(); ++it) 
    {
        for(const ScriptRowData* rd : it.value()) 
        {
            QJsonObject obj;

            obj["original"] = rd->position.originalText;
            obj["file"] = rd->scriptFileName;
            obj["type"] = QJsonArray();
            if(rd->position.type == ScriptTextPosition::Type::RowCol) 
            {
                const auto& pos = std::get<0>(rd->position.d);
                obj["row"] = qsizetype(pos.row);
                obj["col"] = qsizetype(pos.col);
            }
            else if(rd->position.type == ScriptTextPosition::Type::Argument)
            {
                const auto& args = std::get<1>(rd->position.d);
                obj["parameterName"] = args.valueName;
            }

            QJsonObject translates;
            for(const auto& [lang, text] : std::views::zip(_languages, rd->translations))
            {
                translates[lang] = text;
            }
            obj["translates"] = translates;

            jsonArray.append(obj);
        }

    }

    QFile file(editJsonFile);
    if(file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(jsonArray).toJson(QJsonDocument::Indented));
    }
    else {
        return false;
    }

    return true;
}

// ────────────────────────────────────────────────────────────
void ScriptCSVTableModel::saveAllModifiedFiles(const QString& editingDir)
{
    if(_dirtyFiles.isEmpty()) { return; }

    for(const auto& scriptFileName : std::as_const(_dirtyFiles)) 
    {
        const auto csvPath = editingDir + "/" + withoutExtension(scriptFileName) + ".json";

        FastCSVContainer container;
        bool loaded = false;
        if(QFile::exists(csvPath)) {
            loaded = container.loadFromFile(csvPath);
        }
        if(!loaded) {
            std::vector<std::vector<QString>> headerOnly;
            auto& hdr = headerOnly.emplace_back();
            hdr.push_back("original");
            for(const auto& lang : _languages) {
                hdr.push_back(lang);
            }
            container.loadFromCsvData(headerOnly);
        }

        for(const auto& rd : _rows) 
        {
            if(rd.scriptFileName != scriptFileName) { continue; }

            bool found = false;
            const size_t rowCount = container.rowCount();
            for(size_t r = 0; r < rowCount; ++r) {
                QString orig;
                container.getValue(r, size_t(0), orig);
                if(orig == rd.originalText) {
                    for(int i = 0; i < _languages.size(); ++i) {
                        if(i < rd.translations.size()) {
                            container.setValue(r, _languages[i], rd.translations[i]);
                        }
                    }
                    found = true;
                    break;
                }
            }

            if(!found) {
                std::vector<QString> newRow;
                newRow.push_back(rd.originalText);
                for(int i = 0; i < _languages.size(); ++i) {
                    newRow.push_back(i < rd.translations.size() ? rd.translations[i] : QString());
                }
                container.addRow(newRow);
            }
        }

        container.saveToFile(csvPath);
    }
    _dirtyFiles.clear();
}

// ────────────────────────────────────────────────────────────
void ScriptCSVTableModel::setUseLanguageFont(bool use)
{
    if(_useLanguageFont == use) { return; }
    _useLanguageFont = use;
    if(!_rows.isEmpty() && _languages.size() > 0) {
        emit dataChanged(
            index(0, Columns::LangColumnStart),
            index(_rows.size() - 1, columnCount() - 1),
            {Qt::FontRole}
        );
    }
}

// ────────────────────────────────────────────────────────────
QString ScriptCSVTableModel::getOriginalText(int row) const
{
    if(row < 0 || _rows.size() <= row) { return {}; }
    return _rows[row].originalText;
}

QString ScriptCSVTableModel::getScriptFileName(int row) const
{
    if(row < 0 || _rows.size() <= row) { return {}; }
    return _rows[row].scriptFileName;
}

Qt::CheckState ScriptCSVTableModel::getCheckState(int row) const
{
    if(row < 0 || _rows.size() <= row) { return Qt::Unchecked; }
    return _rows[row].include ? Qt::Checked : Qt::Unchecked;
}

// ────────────────────────────────────────────────────────────
void ScriptCSVTableModel::updateCheckStateFromTree(const QString& scriptName, Qt::CheckState state)
{
    auto it = _scriptNameToRows.find(scriptName);
    if(it == _scriptNameToRows.end()) { return; }

    const bool include = (state == Qt::Checked);
    int minRow = INT_MAX;
    int maxRow = -1;
    for(int r : it->second) {
        if(r < _rows.size()) {
            _rows[r].include = include;
            minRow = std::min(minRow, r);
            maxRow = std::max(maxRow, r);
        }
    }

    if(maxRow >= 0) {
        emit dataChanged(index(minRow, 0), index(maxRow, columnCount() - 1),
            {Qt::CheckStateRole, Qt::ForegroundRole});
    }
}

std::vector<int> ScriptCSVTableModel::getRowsForScript(const QString& scriptName) const
{
    auto it = _scriptNameToRows.find(scriptName);
    if(it == _scriptNameToRows.end()) { return {}; }
    return it->second;
}

Qt::CheckState ScriptCSVTableModel::computeTreeCheckState(const QString& scriptFileName) const
{
    int total = 0;
    int checked = 0;
    for(const auto& rd : _rows) {
        if(rd.scriptFileName == scriptFileName) {
            ++total;
            if(rd.include) { 
                ++checked; 
            }
        }
    }
    
    if(total == 0 || checked == 0) { return Qt::Unchecked; }
    
    if(checked == total) { return Qt::Checked; }

    return Qt::PartiallyChecked;
}

// ────────────────────────────────────────────────────────────
// QAbstractTableModel の実装
// ────────────────────────────────────────────────────────────

int ScriptCSVTableModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid()) { return 0; }
    return static_cast<int>(_rows.size());
}

int ScriptCSVTableModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid()) { return 0; }
    return Columns::LangColumnStart + static_cast<int>(_languages.size());
}

QVariant ScriptCSVTableModel::data(const QModelIndex& idx, int role) const
{
    if(!idx.isValid() || idx.row() >= _rows.size()) { return QVariant(); }

    const auto& rd = _rows[idx.row()];
    const int   col = idx.column();

    // フォントロール（言語列のみ）
    if(role == Qt::FontRole) {
        if(_useLanguageFont && _settings && col >= Columns::LangColumnStart) {
            const int li = col - Columns::LangColumnStart;
            if(li < _languages.size()) {
                for(const auto& lang : _settings->languages) {
                    if(lang.languageName == _languages[li]) {
                        auto f = lang.font.fontData;
                        f.setPixelSize(12);
                        return f;
                    }
                }
            }
        }
        return QVariant();
    }

    // 前景色ロール
    else if(role == Qt::ForegroundRole) {
        auto colorMap = ComponentBase::getColorTheme().getTextColorForState();
        if(rd.scriptIgnored) {
            return QBrush(colorMap[Qt::Unchecked]);
        }
        return QBrush(colorMap[rd.include ? Qt::Checked : Qt::Unchecked]);
    }

    // チェック状態ロール（列 0 のみ）
    else if(role == Qt::CheckStateRole) {
        if(col == Columns::Include) {
            return static_cast<int>(rd.include ? Qt::Checked : Qt::Unchecked);
        }
        return QVariant();
    }
    else if(role == DataType::IgnoreScriptItem) {
        if(rd.scriptIgnored) { return true; }
        return rd.include == false;
    }

    if(role != Qt::DisplayRole && role != Qt::EditRole) { return QVariant(); }

    switch(col) {
    case Columns::Include:      return QVariant();          // テキストなし（チェックボックスのみ）
    case Columns::ScriptName:   return rd.scriptDisplayName;
    case Columns::TextPoint:    return rd.textPoint;
    case Columns::OriginalText: return rd.originalText;
    default:
    {
        const int li = col - Columns::LangColumnStart;
        if(li >= 0 && li < rd.translations.size()) {
            return rd.translations[li];
        }
        return QString();
    }
    }
}

bool ScriptCSVTableModel::setData(const QModelIndex& idx, const QVariant& value, int role)
{
    if(!idx.isValid() || idx.row() >= _rows.size()) { return false; }

    auto& rd = _rows[idx.row()];
    const int col = idx.column();

    // ── チェック状態の変更（列 0） ──────────────────────────
    if(role == Qt::CheckStateRole && col == Columns::Include) {
        const auto state = static_cast<Qt::CheckState>(value.toInt());
        const bool ignore = (state == Qt::Unchecked);
        rd.include = !ignore;

        if(_settings) {
            auto& info = _settings->fetchScriptInfo(rd.scriptFileName);
            auto it = std::ranges::find_if(info.lines, [&](const auto& t) {
                return t.type == rd.position.type && t.d == rd.position.d;
                });
            if(it != info.lines.end()) {
                it->ignore = ignore;
            }
        }

        emit dataChanged(idx, idx, {Qt::CheckStateRole, Qt::ForegroundRole});
        emit checkStateChanged(rd.scriptFileName, computeTreeCheckState(rd.scriptFileName));
        return true;
    }

    // ── 言語列の編集（列 Columns::MaxColumns 以降） ─────────────────
    if(role == Qt::EditRole && col >= Columns::LangColumnStart) {
        const int li = col - Columns::LangColumnStart;
        if(li >= 0 && li < _languages.size()) {
            if(rd.translations.size() <= li) {
                rd.translations.resize(li + 1);
            }
            rd.translations[li] = value.toString();
            _dirtyFiles.insert(rd.scriptFileName);
            emit dataChanged(idx, idx, {Qt::DisplayRole, Qt::EditRole});
            return true;
        }
    }

    return false;
}

Qt::ItemFlags ScriptCSVTableModel::flags(const QModelIndex& idx) const
{
    if(!idx.isValid()) { return Qt::NoItemFlags; }

    switch(idx.column()) {
    case Columns::Include:
        return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    case Columns::ScriptName:
    case Columns::TextPoint:
    case Columns::OriginalText:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    default:
        return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
}

QVariant ScriptCSVTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal) { return QVariant(); }

    if(role == Qt::DisplayRole) {
        switch(section) {
        case Columns::Include:      return tr("Include");
        case Columns::ScriptName:   return tr("Script Name");
        case Columns::TextPoint:    return tr("Text Point");
        case Columns::OriginalText: return tr("Original");
        default:
        {
            const int li = section - Columns::LangColumnStart;
            if(0 <= li && li < _languages.size()) {
                const auto& code = _languages[li];
                const auto  disp = languageDisplayName(code);
                if(!disp.isEmpty()) {
                    return disp + "(" + code + ")";
                }
                QLocale locale(code);
                return locale.nativeLanguageName() + "(" + code + ")";
            }
            return QVariant();
        }
        }
    }

    // UserRole: proxy モデルの列フィルタリングに使用する識別子
    if(role == DataType::FilterText) {
        switch(section) {
        case Columns::Include:      return Column_ID_Include;
        case Columns::ScriptName:   return Column_ID_ScriptName;
        case Columns::TextPoint:    return Column_ID_TextPoint;
        case Columns::OriginalText: return Column_ID_Original;
        default:
        {
            const int li = section - Columns::LangColumnStart;
            if(0 <= li && li < _languages.size()) {
                return _languages[li];
            }
            return QVariant();
        }
        }
    }

    return QVariant();
}

void ScriptCSVTableModel::rebuildCache()
{
    _scriptNameToRows.clear();
    for(int r = 0; r < _rows.size(); ++r) {
        _scriptNameToRows[_rows[r].scriptDisplayName].push_back(r);
    }
}
