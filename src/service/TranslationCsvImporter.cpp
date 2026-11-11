#include "TranslationCsvImporter.h"
#include "../ui/editor/FastCSVContainer.h"
#include <QAbstractItemModel>

namespace langscore {

namespace
{
    //改行コードを \n に統一する
    QString ToLf(QString text)
    {
        text.replace("\r\n", "\n");
        text.replace('\r', '\n');
        return text;
    }

    //先頭と末尾の改行を取り除く
    QString TrimLineBreaks(QString text)
    {
        while(text.startsWith('\n')) { text.remove(0, 1); }
        while(text.endsWith('\n')) { text.chop(1); }
        return text;
    }

    //原文の一致確認用。改行の種類と、先頭・末尾の改行の有無は区別しない。
    //以前の divisi は、複数行の文章の末尾または先頭に改行を付けて書き出していたため
    QString ToKey(const QString& text)
    {
        return TrimLineBreaks(ToLf(text));
    }

    //翻訳文の先頭・末尾の改行を、エディターの原文に合わせて取り除く。
    //以前の CSV では、原文と同様に翻訳文にも末尾の改行が付いている
    QString FitEdgeLineBreaks(QString text, const QString& original)
    {
        text = ToLf(text);
        const auto originalLf = ToLf(original);
        if(originalLf.startsWith('\n') == false)
        {
            while(text.startsWith('\n')) { text.remove(0, 1); }
        }
        if(originalLf.endsWith('\n') == false)
        {
            while(text.endsWith('\n')) { text.chop(1); }
        }
        return text;
    }
}

int ImportTranslationCsv(
    const QString& csvPath,
    QAbstractItemModel& model,
    const std::function<QString(int row)>& originalOfRow,
    const QHash<QString, int>& languageColumns,
    const std::function<QString(int row)>& positionOfRow
)
{
    FastCSVContainer imported;
    if(imported.loadFromFile(csvPath) == false) { return -1; }
    if(imported.hasColumn("original") == false) { return -1; }

    QHash<QString, size_t> importedRowOfOriginal;
    for(size_t row = 0; row < imported.rowCount(); ++row) {
        importedRowOfOriginal.insert(ToKey(imported.at(row, "original")), row);
    }

    int changedCells = 0;
    for(int row = 0; row < model.rowCount(); ++row)
    {
        const auto original = originalOfRow(row);
        auto importedRow = importedRowOfOriginal.constFind(ToKey(original));
        if(importedRow == importedRowOfOriginal.cend() && positionOfRow) {
            importedRow = importedRowOfOriginal.constFind(positionOfRow(row));
        }
        if(importedRow == importedRowOfOriginal.cend()) { continue; }

        for(auto column = languageColumns.cbegin(); column != languageColumns.cend(); ++column)
        {
            if(imported.hasColumn(column.key()) == false) { continue; }
            const auto text = FitEdgeLineBreaks(imported.at(importedRow.value(), column.key()), original);
            if(text.isEmpty()) { continue; }

            const auto index = model.index(row, column.value());
            if(model.data(index, Qt::EditRole).toString() == text) { continue; }
            if(model.setData(index, text, Qt::EditRole)) { ++changedCells; }
        }
    }
    return changedCells;
}

} // namespace langscore
