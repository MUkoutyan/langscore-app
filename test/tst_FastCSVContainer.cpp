#include <QTest>
#include "FastCSVContainer.h"

class TestFastCSVContainer : public QObject
{
    Q_OBJECT

private slots:
    void testLoadEmpty();
    void testLoadBasic();
    void testRowCount();
    void testColumnCount();
    void testAtByIndex();
    void testAtByColumnName();
    void testGetValue();
    void testSetValue();
    void testAddRow();
    void testRemoveRow();
    void testAddColumn();
    void testRemoveColumnByIndex();
    void testRemoveColumnByName();
    void testFindRowsWithValue();
    void testFindRowsContaining();
    void testEscapeNoSpecial();
    void testEscapeWithComma();
    void testEscapeWithQuote();
    void testEscapeWithNewline();
    void testUnescapeEmpty();
    void testUnescapeQuoted();
    void testUnescapeDoubleQuote();
    void testHeaders();
    void testHasColumn();
    void testLoadMissingColumns();
};

void TestFastCSVContainer::testLoadEmpty()
{
    langscore::FastCSVContainer csv;
    QVERIFY(csv.loadFromCsvData({}) == false);
    QVERIFY(csv.isEmpty());
}

void TestFastCSVContainer::testLoadBasic()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a", "b", "c"},
        {"d", "e", "f"},
    };
    QVERIFY(csv.loadFromCsvData(data));
    QVERIFY(csv.isEmpty() == false);
}

void TestFastCSVContainer::testRowCount()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
        {"c", "d"},
        {"e", "f"},
    };
    csv.loadFromCsvData(data);
    QCOMPARE(csv.rowCount(), size_t{3});
}

void TestFastCSVContainer::testColumnCount()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a", "b", "c"},
    };
    csv.loadFromCsvData(data);
    QCOMPARE(csv.columnCount(), size_t{3});
}

void TestFastCSVContainer::testAtByIndex()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"hello", "world"},
    };
    csv.loadFromCsvData(data);
    QCOMPARE(csv.at(size_t{0}, size_t{0}), QString("hello"));
    QCOMPARE(csv.at(size_t{0}, size_t{1}), QString("world"));
}

void TestFastCSVContainer::testAtByColumnName()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"name", "value"},
        {"foo", "42"},
    };
    csv.loadFromCsvData(data);
    QCOMPARE(csv.at(size_t{0}, QString("name")), QString("foo"));
    QCOMPARE(csv.at(size_t{0}, QString("value")), QString("42"));
}

void TestFastCSVContainer::testGetValue()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"alpha", "beta"},
    };
    csv.loadFromCsvData(data);

    QString out;
    QVERIFY(csv.getValue(size_t{0}, size_t{0}, out));
    QCOMPARE(out, QString("alpha"));

    QVERIFY(csv.getValue(size_t{0}, size_t{1}, out));
    QCOMPARE(out, QString("beta"));

    QVERIFY(csv.getValue(size_t{1}, size_t{0}, out) == false);
}

void TestFastCSVContainer::testSetValue()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1"},
        {"original"},
    };
    csv.loadFromCsvData(data);
    QVERIFY(csv.setValue(size_t{0}, size_t{0}, QString("modified")));
    QCOMPARE(csv.at(size_t{0}, size_t{0}), QString("modified"));
}

void TestFastCSVContainer::testAddRow()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
    };
    csv.loadFromCsvData(data);
    csv.addRow(std::vector<QString>{"c", "d"});
    QCOMPARE(csv.rowCount(), size_t{2});
    QCOMPARE(csv.at(size_t{1}, size_t{0}), QString("c"));
}

void TestFastCSVContainer::testRemoveRow()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
        {"c", "d"},
    };
    csv.loadFromCsvData(data);
    csv.removeRow(size_t{0});
    QCOMPARE(csv.rowCount(), size_t{1});
    QCOMPARE(csv.at(size_t{0}, size_t{0}), QString("c"));
}

void TestFastCSVContainer::testAddColumn()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1"},
        {"a"},
    };
    csv.loadFromCsvData(data);
    csv.addColumn("col2", "default");
    QCOMPARE(csv.columnCount(), size_t{2});
    QCOMPARE(csv.at(size_t{0}, QString("col2")), QString("default"));
}

void TestFastCSVContainer::testRemoveColumnByIndex()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a", "b", "c"},
    };
    csv.loadFromCsvData(data);
    csv.removeColumn(size_t{1});
    QCOMPARE(csv.columnCount(), size_t{2});
    QVERIFY(csv.hasColumn("col1"));
    QVERIFY(csv.hasColumn("col2") == false);
    QVERIFY(csv.hasColumn("col3"));
}

void TestFastCSVContainer::testRemoveColumnByName()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
    };
    csv.loadFromCsvData(data);
    csv.removeColumn(QString("col1"));
    QCOMPARE(csv.columnCount(), size_t{1});
    QVERIFY(csv.hasColumn("col1") == false);
    QVERIFY(csv.hasColumn("col2"));
}

void TestFastCSVContainer::testFindRowsWithValue()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"key", "value"},
        {"a", "1"},
        {"b", "2"},
        {"a", "3"},
    };
    csv.loadFromCsvData(data);
    auto rows = csv.findRowsWithValue(QString("key"), QString("a"));
    QCOMPARE(rows.size(), size_t{2});
    QCOMPARE(rows[0], size_t{0});
    QCOMPARE(rows[1], size_t{2});
}

void TestFastCSVContainer::testFindRowsContaining()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"text"},
        {"Hello World"},
        {"Goodbye"},
        {"World Peace"},
    };
    csv.loadFromCsvData(data);
    auto rows = csv.findRowsWithValueContaining(QString("text"), QString("World"));
    QCOMPARE(rows.size(), size_t{2});
}

void TestFastCSVContainer::testEscapeNoSpecial()
{
    langscore::FastCSVContainer csv;
    QCOMPARE(csv.escapeCSVField("hello"), QString("hello"));
    QCOMPARE(csv.escapeCSVField(""), QString(""));
}

void TestFastCSVContainer::testEscapeWithComma()
{
    langscore::FastCSVContainer csv;
    QCOMPARE(csv.escapeCSVField("a,b"), QString("\"a,b\""));
}

void TestFastCSVContainer::testEscapeWithQuote()
{
    langscore::FastCSVContainer csv;
    // say "hi" → "say ""hi"""
    QCOMPARE(csv.escapeCSVField("say \"hi\""), QString("\"say \"\"hi\"\"\""));
}

void TestFastCSVContainer::testEscapeWithNewline()
{
    langscore::FastCSVContainer csv;
    QCOMPARE(csv.escapeCSVField("line1\nline2"), QString("\"line1\nline2\""));
}

void TestFastCSVContainer::testUnescapeEmpty()
{
    langscore::FastCSVContainer csv;
    QCOMPARE(csv.unescapeCSVField(""), QString(""));
}

void TestFastCSVContainer::testUnescapeQuoted()
{
    langscore::FastCSVContainer csv;
    // "hello" → hello
    QCOMPARE(csv.unescapeCSVField("\"hello\""), QString("hello"));
}

void TestFastCSVContainer::testUnescapeDoubleQuote()
{
    langscore::FastCSVContainer csv;
    // "say ""hi""" → say "hi"
    QCOMPARE(csv.unescapeCSVField("\"say \"\"hi\"\"\""), QString("say \"hi\""));
}

void TestFastCSVContainer::testHeaders()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"id", "name", "desc"},
        {"1", "Alice", "test"},
    };
    csv.loadFromCsvData(data);
    const auto& headers = csv.headers();
    QCOMPARE(headers.size(), qsizetype{3});
    QCOMPARE(headers[0], QString("id"));
    QCOMPARE(headers[1], QString("name"));
    QCOMPARE(headers[2], QString("desc"));
}

void TestFastCSVContainer::testHasColumn()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
    };
    csv.loadFromCsvData(data);
    QVERIFY(csv.hasColumn("col1"));
    QVERIFY(csv.hasColumn("col2"));
    QVERIFY(csv.hasColumn("col3") == false);
}

void TestFastCSVContainer::testLoadMissingColumns()
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a"},
    };
    csv.loadFromCsvData(data);
    QCOMPARE(csv.columnCount(), size_t{3});

    QString out;
    QVERIFY(csv.getValue(size_t{0}, size_t{2}, out));
    QCOMPARE(out, QString(""));
}

QTEST_MAIN(TestFastCSVContainer)
#include "tst_FastCSVContainer.moc"
