#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include "csv.hpp"

class TestCsv : public QObject
{
    Q_OBJECT

private slots:
    void testParseLineBasic();
    void testParseLineQuotedComma();
    void testReadCsvBasic();
    void testReadCsvMultiLineCell();
    void testReadCsvHeaderOnly();
    void testReadCsvMissingFile();
    void testParseScriptNameRowCol();
    void testParseScriptNameArgument();
    void testParseScriptRowCol();
    void testParseScriptRowColInvalid();
    void testReadCsvUtf8Bom();

    // 不具合の再発防止
    void testParseLineTrailingEmptyColumn();
    void testReadCsvTrailingEmptyColumn();
    void testReadCsvNonAnsiPath();

private:
    QTemporaryDir tempDir;
    QString writeFile(const QString& name, const QByteArray& content);
};

QString TestCsv::writeFile(const QString& name, const QByteArray& content)
{
    auto path = tempDir.filePath(name);
    QFile file(path);
    if(file.open(QFile::WriteOnly) == false){ qFatal("cannot create test file"); }
    file.write(content);
    return path;
}

void TestCsv::testParseLineBasic()
{
    auto cols = langscore::parseCsvLine("a,b,c");
    QCOMPARE(cols.size(), size_t{3});
    QCOMPARE(cols[0], QString("a"));
    QCOMPARE(cols[1], QString("b"));
    QCOMPARE(cols[2], QString("c"));
}

void TestCsv::testParseLineQuotedComma()
{
    // ダブルクォート内のカンマは区切りとして扱わない（クォート自体は保持される）
    auto cols = langscore::parseCsvLine("\"a,b\",c");
    QCOMPARE(cols.size(), size_t{2});
    QCOMPARE(cols[0], QString("\"a,b\""));
    QCOMPARE(cols[1], QString("c"));
}

void TestCsv::testReadCsvBasic()
{
    auto path = writeFile("basic.csv", "original,en\nこんにちは,Hello\n");
    auto csv = langscore::readCsv(path);
    QCOMPARE(csv.size(), size_t{2});
    QCOMPARE(csv[0][0], QString("original"));
    QCOMPARE(csv[0][1], QString("en"));
    QCOMPARE(csv[1][0], QString::fromUtf8("こんにちは"));
    QCOMPARE(csv[1][1], QString("Hello"));
}

void TestCsv::testReadCsvMultiLineCell()
{
    auto path = writeFile("multiline.csv", "id,text\n1,\"line1\nline2\"\n2,hello\n");
    auto csv = langscore::readCsv(path);
    QCOMPARE(csv.size(), size_t{3});
    QCOMPARE(csv[1][0], QString("1"));
    QCOMPARE(csv[1][1], QString("\"line1\nline2\""));
    QCOMPARE(csv[2][1], QString("hello"));
}

void TestCsv::testReadCsvHeaderOnly()
{
    // ヘッダーのみ（2行未満）は空として扱う
    auto path = writeFile("header.csv", "original,en\n");
    QVERIFY(langscore::readCsv(path).empty());
}

void TestCsv::testReadCsvMissingFile()
{
    QVERIFY(langscore::readCsv(tempDir.filePath("not_exists.csv")).empty());
}

void TestCsv::testParseScriptNameRowCol()
{
    auto pos = langscore::parseScriptNameWithRowCol("Script.rb:10:5");
    QCOMPARE(pos.type, langscore::ScriptTextPosition::Type::RowCol);
    auto cell = std::get<langscore::ScriptTextPosition::RowCol>(pos.d);
    QCOMPARE(cell.row, size_t{10});
    QCOMPARE(cell.col, size_t{5});
}

void TestCsv::testParseScriptNameArgument()
{
    auto pos = langscore::parseScriptNameWithRowCol("Plugin.js:messageText");
    QCOMPARE(pos.type, langscore::ScriptTextPosition::Type::Argument);
    QCOMPARE(std::get<langscore::ScriptTextPosition::ScriptArg>(pos.d).valueName, QString("messageText"));
}

void TestCsv::testParseScriptRowCol()
{
    auto [row, col] = langscore::parseScriptWithRowCol("3:4");
    QCOMPARE(row, size_t{3});
    QCOMPARE(col, size_t{4});
}

void TestCsv::testParseScriptRowColInvalid()
{
    constexpr size_t invalid = std::numeric_limits<size_t>::max();
    QCOMPARE(std::get<0>(langscore::parseScriptWithRowCol("abc")), invalid);
    QCOMPARE(std::get<0>(langscore::parseScriptWithRowCol("x:4")), invalid);
    QCOMPARE(std::get<1>(langscore::parseScriptWithRowCol("3:y")), invalid);
}

void TestCsv::testParseLineTrailingEmptyColumn()
{
    // 末尾の空列が落ちず、2列として扱われること
    auto cols = langscore::parseCsvLine("a,");
    QCOMPARE(cols.size(), size_t{2});
    QCOMPARE(cols[1], QString());
}

void TestCsv::testReadCsvTrailingEmptyColumn()
{
    // 翻訳が未入力（最後の列が空）の行も列数が揃うこと
    auto path = writeFile("trailing.csv", "original,en\nHello,\n");
    auto csv = langscore::readCsv(path);
    QCOMPARE(csv.size(), size_t{2});
    QCOMPARE(csv[1].size(), size_t{2});
}

void TestCsv::testReadCsvUtf8Bom()
{
    // BOM 付き UTF-8 の先頭ヘッダーに BOM が残らないこと
    auto path = writeFile("bom.csv", "\xEF\xBB\xBForiginal,en\nA,B\n");
    auto csv = langscore::readCsv(path);
    QCOMPARE(csv.size(), size_t{2});
    QCOMPARE(csv[0][0], QString("original"));
}

void TestCsv::testReadCsvNonAnsiPath()
{
    // ANSI コードページで表現できない文字（ハングル）を含むパスでも読み込めること
    const QString dirName(u"\uD55C\uAE00");
    QVERIFY(QDir(tempDir.path()).mkdir(dirName));
    auto path = writeFile(dirName + "/data.csv", "original,en\nA,B\n");
    QVERIFY(QFile::exists(path));
    QCOMPARE(langscore::readCsv(path).size(), size_t{2});
}

QTEST_MAIN(TestCsv)
#include "tst_Csv.moc"
