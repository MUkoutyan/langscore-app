#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include <QStandardItemModel>
#include "TranslationCsvImporter.h"

//-------------------------------------------------------------------
// 外部エディターで編集したCSVの取り込み
//-------------------------------------------------------------------
namespace
{
    //MSVC は /utf-8 無しでは char の文字列を ANSI にするため、UTF-16 から作る
    QString Text(const char16_t* text)
    {
        return QString::fromUtf16(text);
    }

    //列は original, ja, en
    void AddRow(QStandardItemModel& model, const QString& original, const QString& ja, const QString& en)
    {
        model.appendRow({new QStandardItem(original), new QStandardItem(ja), new QStandardItem(en)});
    }
}

TEST(TranslationCsvImporter, ImportsNonEmptyCellsByOriginal)
{
    QTemporaryDir dir;
    const auto csvPath = dir.filePath("Actors.csv");
    {
        QFile file(csvPath);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        //外部エディターは改行を \r\n で保存することがある。行の並びもエディターと違う
        file.write("\xEF\xBB\xBF");
        file.write(Text(
            u"original,ja,en,fr\r\n"
            u"\"一行目\r\n二行目\",,\"Line1\r\nLine2\",Ligne\r\n"
            u"名前,,Name,Nom\r\n"
            u"無い行,,Missing,\r\n").toUtf8());
    }

    QStandardItemModel model;
    AddRow(model, Text(u"名前"), Text(u"名前"), "OldName");
    AddRow(model, Text(u"一行目\n二行目"), "", "");
    const auto originalOfRow = [&model](int row) { return model.item(row, 0)->text(); };

    const auto changed = langscore::ImportTranslationCsv(csvPath, model, originalOfRow, {{"ja", 1}, {"en", 2}});

    EXPECT_EQ(changed, 2);
    //空のセルは取り込まず、エディターの訳を残す
    EXPECT_EQ(model.item(0, 1)->text(), Text(u"名前"));
    EXPECT_EQ(model.item(0, 2)->text(), "Name");
    //原文の改行の違いは無視して一致させ、セルの改行は \n にする
    EXPECT_EQ(model.item(1, 2)->text(), "Line1\nLine2");
}

//以前の divisi は、複数行の文章の末尾（または先頭）に改行を付けて書き出していた
TEST(TranslationCsvImporter, IgnoresEdgeLineBreaksOfOldCsv)
{
    QTemporaryDir dir;
    const auto csvPath = dir.filePath("CommonEvents.csv");
    {
        QFile file(csvPath);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        file.write(Text(
            u"original,ja,en\n"
            u"\"名前\n「台詞」\n\",,\"Name\nLine\n\"\n"
            u"\"先頭が改行\n\",,\"Leading\n\"\n").toUtf8());
    }

    QStandardItemModel model;
    AddRow(model, Text(u"名前\r\n「台詞」"), "", "");
    AddRow(model, Text(u"\n先頭が改行"), "", "");
    const auto originalOfRow = [&model](int row) { return model.item(row, 0)->text(); };

    const auto changed = langscore::ImportTranslationCsv(csvPath, model, originalOfRow, {{"ja", 1}, {"en", 2}});

    EXPECT_EQ(changed, 2);
    //訳の末尾の改行は、エディターの原文に無いため取り除く
    EXPECT_EQ(model.item(0, 2)->text(), "Name\nLine");
    EXPECT_EQ(model.item(1, 2)->text(), "Leading");
}

TEST(TranslationCsvImporter, RejectsCsvWithoutOriginalColumn)
{
    QTemporaryDir dir;
    const auto csvPath = dir.filePath("Other.csv");
    {
        QFile file(csvPath);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        file.write("a,b\n1,2");
    }
    QStandardItemModel model;
    const auto originalOfRow = [&model](int row) { return model.item(row, 0)->text(); };
    EXPECT_EQ(langscore::ImportTranslationCsv(csvPath, model, originalOfRow, {{"ja", 1}}), -1);
}
