#include "EditingTranslationExporter.h"
#include "../settings.h"
#include "../csv.hpp"
#include "../ui/editor/FastCSVContainer.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHash>
#include <algorithm>

namespace langscore {

namespace
{
    //divisi の書き出し先（config::exportDirectory と同じ）。言語ごとに書き出す場合は言語ごとのフォルダ
    QStringList ExportFolders(const settings& setting)
    {
        const QDir langscoreProjectDir(setting.langscoreProjectDirectory);
        const auto root = QDir::cleanPath(langscoreProjectDir.absoluteFilePath(setting.writeObj.exportDirectory));
        if(setting.writeObj.exportByLanguage == false && setting.writeObj.enableLanguagePatch == false) {
            return {root};
        }
        QStringList folders;
        for(const auto& language : setting.languages) {
            if(language.enable) { folders.append(root + "/" + language.languageName); }
        }
        return folders;
    }

    //原文の一致確認用。editing のCSVは改行を \n にそろえて読むため、書き出したCSV側もそろえる
    QString ToKey(QString text)
    {
        return text.replace("\r\n", "\n");
    }

    //Scripts.csv は、1列目が原文ではなく「ファイル名:行:列」か「ファイル名:パラメータ名」のキーになる。
    //プラグインはこのキーで訳を引くため、キーから原文を引けるようにする
    QHash<QString, QString> ScriptOriginalOfKey(const QString& editJsonPath)
    {
        QHash<QString, QString> originalOfKey;
        QFile file(editJsonPath);
        if(file.open(QIODevice::ReadOnly) == false) { return originalOfKey; }
        for(const auto& value : QJsonDocument::fromJson(file.readAll()).array())
        {
            const auto obj = value.toObject();
            const auto prefix = QFileInfo(obj["file"].toString()).completeBaseName() + ":";
            const auto key = obj.contains("parameterName")
                ? prefix + obj["parameterName"].toString()
                : prefix + QString::number(obj["row"].toInteger()) + ":" + QString::number(obj["col"].toInteger());
            originalOfKey.insert(key, obj["original"].toString());
        }
        return originalOfKey;
    }

    bool ApplyToExportedFile(
        const QString& exportPath,
        const FastCSVContainer& editing,
        const QHash<QString, size_t>& editingRowOfOriginal,
        const QHash<QString, QString>& originalOfKey,
        const settings& setting
    )
    {
        //原文の列はゲーム側のデータと一致確認をするため、読んだ値（クォートを含む）をそのまま書き込む
        auto rows = readCsv(exportPath);
        if(rows.empty() || rows[0].empty()) { return false; }

        //divisi は BOM 付きで書き出す
        auto& firstHeader = rows[0][0];
        if(firstHeader.startsWith(QChar(0xFEFF))) { firstHeader.remove(0, 1); }

        const FastCSVContainer csvFormat;
        QStringList headers;
        for(const auto& field : rows[0]) {
            headers.append(csvFormat.unescapeCSVField(field));
        }

        const bool overwriteAll = setting.writeObj.writeMode == 0;
        const bool isVXAce = setting.projectType == settings::VXAce;
        const auto columnCount = static_cast<size_t>(headers.size());
        for(size_t row = 1; row < rows.size(); ++row)
        {
            auto& fields = rows[row];
            if(fields.empty()) { continue; }
            fields.resize(std::max(fields.size(), columnCount));

            const auto firstField = csvFormat.unescapeCSVField(fields[0]);
            const auto original = originalOfKey.value(firstField, firstField);
            const auto editingRow = editingRowOfOriginal.constFind(ToKey(original));
            for(size_t column = 1; column < columnCount; ++column)
            {
                const auto& language = headers[static_cast<qsizetype>(column)];
                auto text = csvFormat.unescapeCSVField(fields[column]);
                if(editingRow != editingRowOfOriginal.cend() && editing.hasColumn(language))
                {
                    const auto& edited = editing.at(editingRow.value(), language);
                    if(overwriteAll || edited.isEmpty() == false) { text = edited; }
                }
                if(text.isEmpty() && setting.writeObj.enableFillDefaultLanguage && language == setting.defaultLanguage) {
                    text = original;
                }
                fields[column] = csvFormat.escapeCSVField(ToGameLineBreak(text, original, isVXAce));
            }
        }

        //divisi と同じ形（BOM 付き、行の区切りは \n、末尾に改行なし）で書く
        QStringList lines;
        for(const auto& fields : rows) {
            lines.append(QStringList(fields.begin(), fields.end()).join(','));
        }
        QFile file(exportPath);
        if(file.open(QIODevice::WriteOnly) == false) { return false; }
        file.write("\xEF\xBB\xBF");
        file.write(lines.join('\n').toUtf8());
        return true;
    }
}

QString ToGameLineBreak(QString text, const QString& original, bool isVXAce)
{
    text.replace("\r\n", "\n");
    //VXAce でも、イベントの文章は divisi が行を \n で結合するため、原文に合わせる
    const bool useCrLf = original.contains("\r\n") || (original.contains('\n') == false && isVXAce);
    if(useCrLf) { text.replace("\n", "\r\n"); }
    return text;
}

QStringList ApplyEditingTranslations(const settings& setting)
{
    const auto editingFolder = setting.langscoreProjectDirectory + "/editing";
    const auto exportFolders = ExportFolders(setting);
    QStringList updatedFiles;
    for(const auto& fileName : QDir(editingFolder).entryList({"*.csv"}, QDir::Files))
    {
        FastCSVContainer editing;
        if(editing.loadFromFile(editingFolder + "/" + fileName) == false) { continue; }
        if(editing.hasColumn("original") == false) { continue; }

        //原文からアプリ側エディターの行を参照できるようにする
        QHash<QString, size_t> editingRowOfOriginal;
        for(size_t row = 0; row < editing.rowCount(); ++row) {
            editingRowOfOriginal.insert(ToKey(editing.at(row, "original")), row);
        }

        const auto originalOfKey = fileName == "Scripts.csv"
            ? ScriptOriginalOfKey(editingFolder + "/Scripts.lsjson")
            : QHash<QString, QString>{};
        for(const auto& folder : exportFolders)
        {
            const auto exportPath = folder + "/" + fileName;
            if(QFile::exists(exportPath) == false) { continue; }
            if(ApplyToExportedFile(exportPath, editing, editingRowOfOriginal, originalOfKey, setting)) {
                updatedFiles.append(exportPath);
            }
        }
    }
    return updatedFiles;
}

} // namespace langscore
