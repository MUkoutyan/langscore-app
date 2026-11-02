#include "test_common.h"
#include "FastCSVContainer.h"

TEST(FastCSVContainer, LoadEmpty)
{
    langscore::FastCSVContainer csv;
    EXPECT_FALSE(csv.loadFromCsvData({}));
    EXPECT_TRUE(csv.isEmpty());
}

TEST(FastCSVContainer, LoadBasic)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a", "b", "c"},
        {"d", "e", "f"},
    };
    EXPECT_TRUE(csv.loadFromCsvData(data));
    EXPECT_FALSE(csv.isEmpty());
}

TEST(FastCSVContainer, RowCount)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
        {"c", "d"},
        {"e", "f"},
    };
    csv.loadFromCsvData(data);
    EXPECT_EQ(csv.rowCount(), size_t{3});
}

TEST(FastCSVContainer, ColumnCount)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a", "b", "c"},
    };
    csv.loadFromCsvData(data);
    EXPECT_EQ(csv.columnCount(), size_t{3});
}

TEST(FastCSVContainer, AtByIndex)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"hello", "world"},
    };
    csv.loadFromCsvData(data);
    EXPECT_EQ(csv.at(size_t{0}, size_t{0}), QString("hello"));
    EXPECT_EQ(csv.at(size_t{0}, size_t{1}), QString("world"));
}

TEST(FastCSVContainer, AtByColumnName)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"name", "value"},
        {"foo", "42"},
    };
    csv.loadFromCsvData(data);
    EXPECT_EQ(csv.at(size_t{0}, QString("name")), QString("foo"));
    EXPECT_EQ(csv.at(size_t{0}, QString("value")), QString("42"));
}

TEST(FastCSVContainer, GetValue)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"alpha", "beta"},
    };
    csv.loadFromCsvData(data);

    QString out;
    EXPECT_TRUE(csv.getValue(size_t{0}, size_t{0}, out));
    EXPECT_EQ(out, QString("alpha"));

    EXPECT_TRUE(csv.getValue(size_t{0}, size_t{1}, out));
    EXPECT_EQ(out, QString("beta"));

    EXPECT_FALSE(csv.getValue(size_t{1}, size_t{0}, out));
}

TEST(FastCSVContainer, SetValue)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1"},
        {"original"},
    };
    csv.loadFromCsvData(data);
    EXPECT_TRUE(csv.setValue(size_t{0}, size_t{0}, QString("modified")));
    EXPECT_EQ(csv.at(size_t{0}, size_t{0}), QString("modified"));
}

TEST(FastCSVContainer, AddRow)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
    };
    csv.loadFromCsvData(data);
    csv.addRow(std::vector<QString>{"c", "d"});
    EXPECT_EQ(csv.rowCount(), size_t{2});
    EXPECT_EQ(csv.at(size_t{1}, size_t{0}), QString("c"));
}

TEST(FastCSVContainer, RemoveRow)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
        {"c", "d"},
    };
    csv.loadFromCsvData(data);
    csv.removeRow(size_t{0});
    EXPECT_EQ(csv.rowCount(), size_t{1});
    EXPECT_EQ(csv.at(size_t{0}, size_t{0}), QString("c"));
}

TEST(FastCSVContainer, AddColumn)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1"},
        {"a"},
    };
    csv.loadFromCsvData(data);
    csv.addColumn("col2", "default");
    EXPECT_EQ(csv.columnCount(), size_t{2});
    EXPECT_EQ(csv.at(size_t{0}, QString("col2")), QString("default"));
}

TEST(FastCSVContainer, RemoveColumnByIndex)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a", "b", "c"},
    };
    csv.loadFromCsvData(data);
    csv.removeColumn(size_t{1});
    EXPECT_EQ(csv.columnCount(), size_t{2});
    EXPECT_TRUE(csv.hasColumn("col1"));
    EXPECT_FALSE(csv.hasColumn("col2"));
    EXPECT_TRUE(csv.hasColumn("col3"));
}

TEST(FastCSVContainer, RemoveColumnByName)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
    };
    csv.loadFromCsvData(data);
    csv.removeColumn(QString("col1"));
    EXPECT_EQ(csv.columnCount(), size_t{1});
    EXPECT_FALSE(csv.hasColumn("col1"));
    EXPECT_TRUE(csv.hasColumn("col2"));
}

TEST(FastCSVContainer, FindRowsWithValue)
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
    ASSERT_EQ(rows.size(), size_t{2});
    EXPECT_EQ(rows[0], size_t{0});
    EXPECT_EQ(rows[1], size_t{2});
}

TEST(FastCSVContainer, FindRowsContaining)
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
    EXPECT_EQ(rows.size(), size_t{2});
}

TEST(FastCSVContainer, EscapeNoSpecial)
{
    langscore::FastCSVContainer csv;
    EXPECT_EQ(csv.escapeCSVField("hello"), QString("hello"));
    EXPECT_EQ(csv.escapeCSVField(""), QString(""));
}

TEST(FastCSVContainer, EscapeWithComma)
{
    langscore::FastCSVContainer csv;
    EXPECT_EQ(csv.escapeCSVField("a,b"), QString("\"a,b\""));
}

TEST(FastCSVContainer, EscapeWithQuote)
{
    langscore::FastCSVContainer csv;
    // say "hi" → "say ""hi"""
    EXPECT_EQ(csv.escapeCSVField("say \"hi\""), QString("\"say \"\"hi\"\"\""));
}

TEST(FastCSVContainer, EscapeWithNewline)
{
    langscore::FastCSVContainer csv;
    EXPECT_EQ(csv.escapeCSVField("line1\nline2"), QString("\"line1\nline2\""));
}

TEST(FastCSVContainer, UnescapeEmpty)
{
    langscore::FastCSVContainer csv;
    EXPECT_EQ(csv.unescapeCSVField(""), QString(""));
}

TEST(FastCSVContainer, UnescapeQuoted)
{
    langscore::FastCSVContainer csv;
    // "hello" → hello
    EXPECT_EQ(csv.unescapeCSVField("\"hello\""), QString("hello"));
}

TEST(FastCSVContainer, UnescapeDoubleQuote)
{
    langscore::FastCSVContainer csv;
    // "say ""hi""" → say "hi"
    EXPECT_EQ(csv.unescapeCSVField("\"say \"\"hi\"\"\""), QString("say \"hi\""));
}

TEST(FastCSVContainer, Headers)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"id", "name", "desc"},
        {"1", "Alice", "test"},
    };
    csv.loadFromCsvData(data);
    const auto& headers = csv.headers();
    ASSERT_EQ(headers.size(), qsizetype{3});
    EXPECT_EQ(headers[0], QString("id"));
    EXPECT_EQ(headers[1], QString("name"));
    EXPECT_EQ(headers[2], QString("desc"));
}

TEST(FastCSVContainer, HasColumn)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2"},
        {"a", "b"},
    };
    csv.loadFromCsvData(data);
    EXPECT_TRUE(csv.hasColumn("col1"));
    EXPECT_TRUE(csv.hasColumn("col2"));
    EXPECT_FALSE(csv.hasColumn("col3"));
}

TEST(FastCSVContainer, LoadMissingColumns)
{
    langscore::FastCSVContainer csv;
    std::vector<std::vector<QString>> data = {
        {"col1", "col2", "col3"},
        {"a"},
    };
    csv.loadFromCsvData(data);
    EXPECT_EQ(csv.columnCount(), size_t{3});

    QString out;
    EXPECT_TRUE(csv.getValue(size_t{0}, size_t{2}, out));
    EXPECT_EQ(out, QString(""));
}
