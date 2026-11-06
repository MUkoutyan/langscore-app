#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "csv.hpp"

class CsvTest : public ::testing::Test
{
protected:
    QTemporaryDir tempDir;

    QString WriteFile(const QString& name, const QByteArray& content)
    {
        auto path = tempDir.filePath(name);
        QFile file(path);
        if(file.open(QFile::WriteOnly) == false)
        {
            ADD_FAILURE() << "cannot create test file: " << path.toStdString();
            return path;
        }
        file.write(content);
        return path;
    }
};

TEST_F(CsvTest, ParseLineBasic)
{
    auto cols = langscore::parseCsvLine("a,b,c");
    ASSERT_EQ(cols.size(), size_t{3});
    EXPECT_EQ(cols[0], QString("a"));
    EXPECT_EQ(cols[1], QString("b"));
    EXPECT_EQ(cols[2], QString("c"));
}

TEST_F(CsvTest, ParseLineQuotedComma)
{
    // ダブルクォート内のカンマは区切りとして扱わない（クォート自体は保持される）
    auto cols = langscore::parseCsvLine("\"a,b\",c");
    ASSERT_EQ(cols.size(), size_t{2});
    EXPECT_EQ(cols[0], QString("\"a,b\""));
    EXPECT_EQ(cols[1], QString("c"));
}

TEST_F(CsvTest, ReadCsvBasic)
{
    auto path = WriteFile("basic.csv", "original,en\nこんにちは,Hello\n");
    auto csv = langscore::readCsv(path);
    ASSERT_EQ(csv.size(), size_t{2});
    EXPECT_EQ(csv[0][0], QString("original"));
    EXPECT_EQ(csv[0][1], QString("en"));
    EXPECT_EQ(csv[1][0], QString::fromUtf8("こんにちは"));
    EXPECT_EQ(csv[1][1], QString("Hello"));
}

TEST_F(CsvTest, ReadCsvMultiLineCell)
{
    auto path = WriteFile("multiline.csv", "id,text\n1,\"line1\nline2\"\n2,hello\n");
    auto csv = langscore::readCsv(path);
    ASSERT_EQ(csv.size(), size_t{3});
    EXPECT_EQ(csv[1][0], QString("1"));
    EXPECT_EQ(csv[1][1], QString("\"line1\nline2\""));
    EXPECT_EQ(csv[2][1], QString("hello"));
}

TEST_F(CsvTest, ReadCsvHeaderOnly)
{
    // ヘッダーのみ（2行未満）は空として扱う
    auto path = WriteFile("header.csv", "original,en\n");
    EXPECT_TRUE(langscore::readCsv(path).empty());
}

TEST_F(CsvTest, ReadCsvMissingFile)
{
    EXPECT_TRUE(langscore::readCsv(tempDir.filePath("not_exists.csv")).empty());
}

TEST_F(CsvTest, ParseScriptNameRowCol)
{
    auto pos = langscore::parseScriptNameWithRowCol("Script.rb:10:5");
    ASSERT_EQ(pos.type, langscore::ScriptTextPosition::Type::RowCol);
    auto cell = std::get<langscore::ScriptTextPosition::RowCol>(pos.d);
    EXPECT_EQ(cell.row, size_t{10});
    EXPECT_EQ(cell.col, size_t{5});
}

TEST_F(CsvTest, ParseScriptNameArgument)
{
    auto pos = langscore::parseScriptNameWithRowCol("Plugin.js:messageText");
    ASSERT_EQ(pos.type, langscore::ScriptTextPosition::Type::Argument);
    EXPECT_EQ(std::get<langscore::ScriptTextPosition::ScriptArg>(pos.d).valueName, QString("messageText"));
}

TEST_F(CsvTest, ParseScriptRowCol)
{
    auto [row, col] = langscore::parseScriptWithRowCol("3:4");
    EXPECT_EQ(row, size_t{3});
    EXPECT_EQ(col, size_t{4});
}

TEST_F(CsvTest, ParseScriptRowColInvalid)
{
    constexpr size_t invalid = std::numeric_limits<size_t>::max();
    EXPECT_EQ(std::get<0>(langscore::parseScriptWithRowCol("abc")), invalid);
    EXPECT_EQ(std::get<0>(langscore::parseScriptWithRowCol("x:4")), invalid);
    EXPECT_EQ(std::get<1>(langscore::parseScriptWithRowCol("3:y")), invalid);
}

TEST_F(CsvTest, ReadCsvUtf8Bom)
{
    // BOM 付き UTF-8 の先頭ヘッダーに BOM が残らないこと
    auto path = WriteFile("bom.csv", "\xEF\xBB\xBForiginal,en\nA,B\n");
    auto csv = langscore::readCsv(path);
    ASSERT_EQ(csv.size(), size_t{2});
    EXPECT_EQ(csv[0][0], QString("original"));
}

// 不具合の再発防止
TEST_F(CsvTest, ParseLineTrailingEmptyColumn)
{
    // 末尾の空列が落ちず、2列として扱われること
    auto cols = langscore::parseCsvLine("a,");
    ASSERT_EQ(cols.size(), size_t{2});
    EXPECT_EQ(cols[1], QString());
}

TEST_F(CsvTest, ReadCsvTrailingEmptyColumn)
{
    // 翻訳が未入力（最後の列が空）の行も列数が揃うこと
    auto path = WriteFile("trailing.csv", "original,en\nHello,\n");
    auto csv = langscore::readCsv(path);
    ASSERT_EQ(csv.size(), size_t{2});
    EXPECT_EQ(csv[1].size(), size_t{2});
}

TEST_F(CsvTest, ReadCsvNonAnsiPath)
{
    // ANSI コードページで表現できない文字（ハングル）を含むパスでも読み込めること
    const QString dirName(u"한글");
    ASSERT_TRUE(QDir(tempDir.path()).mkdir(dirName));
    auto path = WriteFile(dirName + "/data.csv", "original,en\nA,B\n");
    ASSERT_TRUE(QFile::exists(path));
    EXPECT_EQ(langscore::readCsv(path).size(), size_t{2});
}
