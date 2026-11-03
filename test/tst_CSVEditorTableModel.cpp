#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include "CSVEditorTableModel.h"
#include "EditorTableDefines.h"

class CSVEditorTableModelTest : public ::testing::Test
{
protected:
    QTemporaryDir tempDir;

    QString WriteJson(const QByteArray& content)
    {
        auto path = tempDir.filePath("Actors.json");
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

TEST_F(CSVEditorTableModelTest, LoadJson)
{
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": ["name"]}])")));
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.index(0, 0).data().toString(), QString("Hello"));
}

// 不具合の再発防止
TEST_F(CSVEditorTableModelTest, LoadJsonKeepsQuotes)
{
    // JSON の文字列は CSV エスケープされていないので、引用符がそのまま残ること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "\"Hello\"", "type": []},
        {"original": "a\"\"b", "type": []}
    ])")));
    EXPECT_EQ(model.index(0, 0).data().toString(), QString("\"Hello\""));
    EXPECT_EQ(model.index(1, 0).data().toString(), QString("a\"\"b"));
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(CSVEditorTableModelTest, SetDataRejectsReadOnlyColumn) // [1]
{
    // 編集できない original・type 列は、setData で書き換えられないこと
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": ["name"]}])")));
    EXPECT_FALSE(model.setData(model.index(0, 0), "Changed"));
    EXPECT_FALSE(model.setData(model.index(0, 1), "Changed"));
    EXPECT_EQ(model.getCell(0, 0).value(), QString("Hello"));
    EXPECT_EQ(model.getCell(0, 1).value(), QString("name"));

    // 言語列は今までどおり編集できること
    model.insertColumn(1, "en");
    EXPECT_TRUE(model.setData(model.index(0, 1), "Hi"));
    EXPECT_EQ(model.getCell(0, 1).value(), QString("Hi"));
}

TEST_F(CSVEditorTableModelTest, TypeColumnWithMultipleTypes) // [14]
{
    // 種類が複数ある行や未知の種類の行でも、Type 列が空欄にならないこと
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "A", "type": ["name", "description"]},
        {"original": "B", "type": ["unknownType"]}
    ])")));
    EXPECT_EQ(model.index(0, 1).data().toString(), QString("name, description"));
    EXPECT_EQ(model.index(1, 1).data().toString(), QString("unknownType"));
}

// 異常終了する可能性があるため最後に実行する
TEST_F(CSVEditorTableModelTest, IsLanguageColumnWithoutSettings)
{
    // settings 未設定や範囲外の列でも、IsLanguageColumn の問い合わせで落ちないこと
    // （MainCSVTable は setSettings より前に proxy へ setSourceModel している）
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    EXPECT_FALSE(model.headerData(0, Qt::Horizontal, DataType::IsLanguageColumn).toBool());
    EXPECT_FALSE(model.headerData(99, Qt::Horizontal, DataType::IsLanguageColumn).toBool());
}
