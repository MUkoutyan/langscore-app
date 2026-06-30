#include "ScriptTableViewModel.h"

#include <QFile>
#include <QBrush>
#include <QLocale>
#include <ranges>
#include <algorithm>

#include "../../settings.h"
#include "../../utility.hpp"
#include "../ComponentBase.h"
#include "../csveditor/FastCSVContainer.h"
#include "service/LanguageNames.h"

using namespace langscore;

// ────────────────────────────────────────────────────────────
ScriptTableViewModel::ScriptTableViewModel(QObject* parent)
    : QAbstractTableModel(parent)
{}

void ScriptTableViewModel::setSettings(std::shared_ptr<settings> setting)
{
    _settings = std::move(setting);
}

// ────────────────────────────────────────────────────────────
void ScriptTableViewModel::loadFromSettings(const QString& editingDir, bool showAllContents)
{
    beginResetModel();
    _rows.clear();
    _languages.clear();
    _dirtyFiles.clear();
    _scriptNameToRows.clear();

    if(!_settings) {
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

    for(const auto& scriptData : scripts) 
    {
        // showAllContents=false のとき、スクリプトレベルで無視されているものを除外
        if(!showAllContents && scriptData.isIgnore()) {
            continue;
        }

        // editing CSV を読み込み (original → translations の対応を構築)
        // 同じ original が複数行ある場合に順序を保持するため vector を使用
        QVector<QPair<QString, QVector<QString>>> csvRows;
        const auto csvPath = editingDir + "/" + withoutExtension(scriptData.fileName) + ".csv";
        if(QFile::exists(csvPath)) {
            FastCSVContainer container;
            if(container.loadFromFile(csvPath)) {
                const size_t count = container.rowCount();
                for(size_t r = 0; r < count; ++r) {
                    QString orig;
                    container.getValue(r, size_t(0), orig);
                    QVector<QString> trans;
                    trans.reserve(_languages.size());
                    for(const auto& lang : _languages) {
                        QString val;
                        container.getValue(r, lang, val);
                        trans.append(val);
                    }
                    csvRows.append({orig, trans});
                }
            }
        }

        // 表示名（拡張子なし）
        auto displayName = scriptData.scriptName;
        if(displayName.contains(scriptExt)) {
            displayName.chop(scriptExt.length());
        }

        for(const auto& line : scriptData.lines) {
            // 行レベルで無視されている行は常に非表示
            if(line.ignore) { continue; }

            RowData rd;
            rd.include           = true;
            rd.scriptIgnored     = scriptData.ignore; // スクリプトレベルの無視フラグ
            rd.scriptDisplayName = scriptData.scriptName; // キャッシュキー兼表示名
            rd.scriptFileName    = scriptData.fileName;
            rd.originalText      = line.value;

            // TextPoint 文字列を構築
            if(std::holds_alternative<TextPosition::RowCol>(line.d)) {
                const auto& cell = std::get<TextPosition::RowCol>(line.d);
                if(cell.row == 0 && cell.col == 0) {
                    rd.textPoint = tr("Parameter");
                } else {
                    rd.textPoint = QString("%1:%2").arg(cell.row).arg(cell.col);
                }
            } else {
                const auto& cell = std::get<TextPosition::ScriptArg>(line.d);
                rd.textPoint = cell.valueName;
            }
            rd.position = line;

            // original テキストで翻訳を検索（最初の一致を使用）
            rd.translations.resize(_languages.size());
            for(const auto& [orig, trans] : csvRows) {
                if(orig == line.value) {
                    rd.translations = trans;
                    break;
                }
            }

            const int newRow = static_cast<int>(_rows.size());
            _rows.append(rd);
            _scriptNameToRows[rd.scriptDisplayName].push_back(newRow);
        }
    }

    endResetModel();
}

// ────────────────────────────────────────────────────────────
void ScriptTableViewModel::saveAllModifiedFiles(const QString& editingDir)
{
    if(_dirtyFiles.isEmpty()) { return; }

    for(const auto& scriptFileName : std::as_const(_dirtyFiles)) {
        const auto csvPath = editingDir + "/"
                           + withoutExtension(scriptFileName) + ".csv";

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

        for(const auto& rd : _rows) {
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
void ScriptTableViewModel::setUseLanguageFont(bool use)
{
    if(_useLanguageFont == use) { return; }
    _useLanguageFont = use;
    if(!_rows.isEmpty() && _languages.size() > 0) {
        emit dataChanged(index(0, FIXED_COLS),
                         index(_rows.size() - 1, columnCount() - 1),
                         {Qt::FontRole});
    }
}

// ────────────────────────────────────────────────────────────
QString ScriptTableViewModel::getOriginalText(int row) const
{
    if(row < 0 || row >= _rows.size()) { return {}; }
    return _rows[row].originalText;
}

QString ScriptTableViewModel::getScriptFileName(int row) const
{
    if(row < 0 || row >= _rows.size()) { return {}; }
    return _rows[row].scriptFileName;
}

Qt::CheckState ScriptTableViewModel::getCheckState(int row) const
{
    if(row < 0 || row >= _rows.size()) { return Qt::Unchecked; }
    return _rows[row].include ? Qt::Checked : Qt::Unchecked;
}

// ────────────────────────────────────────────────────────────
void ScriptTableViewModel::updateCheckStateFromTree(const QString& scriptName, Qt::CheckState state)
{
    auto it = _scriptNameToRows.find(scriptName);
    if(it == _scriptNameToRows.end()) { return; }

    const bool include = (state == Qt::Checked);
    int minRow = INT_MAX, maxRow = -1;
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

std::vector<int> ScriptTableViewModel::getRowsForScript(const QString& scriptName) const
{
    auto it = _scriptNameToRows.find(scriptName);
    if(it == _scriptNameToRows.end()) { return {}; }
    return it->second;
}

Qt::CheckState ScriptTableViewModel::computeTreeCheckState(const QString& scriptFileName) const
{
    int total   = 0;
    int checked = 0;
    for(const auto& rd : _rows) {
        if(rd.scriptFileName == scriptFileName) {
            ++total;
            if(rd.include) { ++checked; }
        }
    }
    if(total == 0 || checked == 0)   { return Qt::Unchecked; }
    if(checked == total)             { return Qt::Checked; }
    return Qt::PartiallyChecked;
}

// ────────────────────────────────────────────────────────────
// QAbstractTableModel の実装
// ────────────────────────────────────────────────────────────

int ScriptTableViewModel::rowCount(const QModelIndex& parent) const
{
    if(parent.isValid()) { return 0; }
    return static_cast<int>(_rows.size());
}

int ScriptTableViewModel::columnCount(const QModelIndex& parent) const
{
    if(parent.isValid()) { return 0; }
    return FIXED_COLS + static_cast<int>(_languages.size());
}

QVariant ScriptTableViewModel::data(const QModelIndex& idx, int role) const
{
    if(!idx.isValid() || idx.row() >= _rows.size()) { return QVariant(); }

    const auto& rd  = _rows[idx.row()];
    const int   col = idx.column();

    // フォントロール（言語列のみ）
    if(role == Qt::FontRole) {
        if(_useLanguageFont && _settings && col >= FIXED_COLS) {
            const int li = col - FIXED_COLS;
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
    if(role == Qt::ForegroundRole) {
        auto colorMap = ComponentBase::getColorTheme().getTextColorForState();
        if(rd.scriptIgnored) {
            return QBrush(colorMap[Qt::Unchecked]);
        }
        return QBrush(colorMap[rd.include ? Qt::Checked : Qt::Unchecked]);
    }

    // チェック状態ロール（列 0 のみ）
    if(role == Qt::CheckStateRole) {
        if(col == COL_INCLUDE) {
            return static_cast<int>(rd.include ? Qt::Checked : Qt::Unchecked);
        }
        return QVariant();
    }

    if(role != Qt::DisplayRole && role != Qt::EditRole) { return QVariant(); }

    switch(col) {
    case COL_INCLUDE:     return QVariant();          // テキストなし（チェックボックスのみ）
    case COL_SCRIPT_NAME: return rd.scriptDisplayName;
    case COL_TEXT_POINT:  return rd.textPoint;
    default:
        {
            const int li = col - FIXED_COLS;
            if(li >= 0 && li < rd.translations.size()) {
                return rd.translations[li];
            }
            return QString();
        }
    }
}

bool ScriptTableViewModel::setData(const QModelIndex& idx, const QVariant& value, int role)
{
    if(!idx.isValid() || idx.row() >= _rows.size()) { return false; }

    auto& rd       = _rows[idx.row()];
    const int col  = idx.column();

    // ── チェック状態の変更（列 0） ──────────────────────────
    if(role == Qt::CheckStateRole && col == COL_INCLUDE) {
        const auto state  = static_cast<Qt::CheckState>(value.toInt());
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

    // ── 言語列の編集（列 FIXED_COLS 以降） ─────────────────
    if(role == Qt::EditRole && col >= FIXED_COLS) {
        const int li = col - FIXED_COLS;
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

Qt::ItemFlags ScriptTableViewModel::flags(const QModelIndex& idx) const
{
    if(!idx.isValid()) { return Qt::NoItemFlags; }

    switch(idx.column()) {
    case COL_INCLUDE:
        return Qt::ItemIsUserCheckable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    case COL_SCRIPT_NAME:
    case COL_TEXT_POINT:
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    default:
        return Qt::ItemIsEditable | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
}

QVariant ScriptTableViewModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if(orientation != Qt::Horizontal) { return QVariant(); }

    if(role == Qt::DisplayRole) {
        switch(section) {
        case COL_INCLUDE:     return tr("Include");
        case COL_SCRIPT_NAME: return tr("Script Name");
        case COL_TEXT_POINT:  return tr("Text Point");
        default:
            {
                const int li = section - FIXED_COLS;
                if(li >= 0 && li < _languages.size()) {
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
    if(role == Qt::UserRole) {
        switch(section) {
        case COL_INCLUDE:     return QString("include");
        case COL_SCRIPT_NAME: return QString("scriptName");
        case COL_TEXT_POINT:  return QString("textPoint");
        default:
            {
                const int li = section - FIXED_COLS;
                if(li >= 0 && li < _languages.size()) {
                    return _languages[li];
                }
                return QVariant();
            }
        }
    }

    return QVariant();
}

void ScriptTableViewModel::rebuildCache()
{
    _scriptNameToRows.clear();
    for(int r = 0; r < _rows.size(); ++r) {
        _scriptNameToRows[_rows[r].scriptDisplayName].push_back(r);
    }
}
