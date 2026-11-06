#include "OldTranslations.h"
#include "../ui/editor/FastCSVContainer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>

namespace langscore
{
namespace
{
    //CSV を読むと原文の改行 \r\n が \n になるため、改行をそろえてから比較する
    QString ToKey(QString text) { return text.replace("\r\n", "\n"); }

    bool SaveRows(const QString& path, const QStringList& headers, const std::vector<QStringList>& rows)
    {
        if(rows.empty()) {
            return QFile::exists(path) == false || QFile::remove(path);
        }
        std::vector<std::vector<QString>> csvData;
        csvData.emplace_back(headers.begin(), headers.end());
        for(const auto& row : rows) { csvData.emplace_back(row.begin(), row.end()); }
        QDir().mkpath(QFileInfo(path).absolutePath());
        FastCSVContainer container;
        return container.loadFromCsvData(csvData) && container.saveToFile(path);
    }
}

QString OldTranslationsPath(const QString& editingCsvPath)
{
    const QFileInfo info(editingCsvPath);
    return info.absolutePath() + "/old/" + info.fileName();
}

int KeepOldTranslations(const QString& editingCsvPath, const QSet<QString>& currentOriginals)
{
    FastCSVContainer editing;
    if(editing.loadFromFile(editingCsvPath) == false || editing.hasColumn("original") == false) { return 0; }

    QSet<QString> currentKeys;
    for(const auto& original : currentOriginals) { currentKeys.insert(ToKey(original)); }

    //すでに残している古い翻訳
    const auto oldPath = OldTranslationsPath(editingCsvPath);
    QStringList headers{"original"};
    std::vector<QStringList> rows;
    QHash<QString, size_t> rowOfOriginal;
    FastCSVContainer old;
    if(QFile::exists(oldPath) && old.loadFromFile(oldPath) && old.hasColumn("original"))
    {
        headers = old.headers();
        for(size_t row = 0; row < old.rowCount(); ++row) {
            rowOfOriginal.insert(ToKey(old.at(row, QString("original"))), rows.size());
            rows.push_back(old.getRow(row));
        }
    }

    int moved = 0;
    for(size_t row = 0; row < editing.rowCount(); ++row)
    {
        const auto& original = editing.at(row, QString("original"));
        if(currentKeys.contains(ToKey(original))) { continue; }

        //原文と同じ文（既定の言語の列）だけの行は、残す訳が無い
        QHash<QString, QString> translations;
        for(const auto& language : editing.headers())
        {
            if(language == "original" || language == "type") { continue; }
            const auto& text = editing.at(row, language);
            if(text.isEmpty() == false && text != original) { translations.insert(language, text); }
        }
        if(translations.isEmpty()) { continue; }

        for(const auto& language : editing.headers()) {
            if(language == "type" || headers.contains(language)) { continue; }
            headers.append(language);
            for(auto& oldRow : rows) { oldRow.append(QString()); }
        }
        auto found = rowOfOriginal.constFind(ToKey(original));
        if(found == rowOfOriginal.cend()) {
            found = rowOfOriginal.insert(ToKey(original), rows.size());
            rows.emplace_back(QStringList(headers.size()));
        }
        auto& target = rows[found.value()];
        target.resize(headers.size());
        target[0] = original;
        for(qsizetype column = 1; column < headers.size(); ++column) {
            const auto& language = headers[column];
            if(editing.hasColumn(language)) { target[column] = editing.at(row, language); }
        }
        ++moved;
    }
    if(moved == 0) { return 0; }
    return SaveRows(oldPath, headers, rows) ? moved : 0;
}

bool RemoveOldTranslations(const QString& editingCsvPath, const QStringList& originals)
{
    const auto oldPath = OldTranslationsPath(editingCsvPath);
    if(QFile::exists(oldPath) == false) { return true; }
    if(originals.isEmpty()) { return QFile::remove(oldPath); }

    FastCSVContainer old;
    if(old.loadFromFile(oldPath) == false || old.hasColumn("original") == false) { return false; }
    QSet<QString> removeKeys;
    for(const auto& original : originals) { removeKeys.insert(ToKey(original)); }
    std::vector<QStringList> rows;
    for(size_t row = 0; row < old.rowCount(); ++row) {
        if(removeKeys.contains(ToKey(old.at(row, QString("original")))) == false) { rows.push_back(old.getRow(row)); }
    }
    return SaveRows(oldPath, old.headers(), rows);
}

bool RemoveAllOldTranslations(const QString& editingFolder)
{
    QDir oldFolder(editingFolder + "/old");
    return oldFolder.exists() == false || oldFolder.removeRecursively();
}
}
