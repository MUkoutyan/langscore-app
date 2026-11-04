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
    // JSON の文字列はCSVエスケープされていないので、引用符がそのまま残ること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "\"Hello\"", "type": []},
        {"original": "a\"\"b", "type": []}
    ])")));
    EXPECT_EQ(model.index(0, 0).data().toString(), QString("\"Hello\""));
    EXPECT_EQ(model.index(1, 0).data().toString(), QString("a\"\"b"));
}

TEST_F(CSVEditorTableModelTest, SetDataRejectsReadOnlyColumn)
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

TEST_F(CSVEditorTableModelTest, TypeColumnWithMultipleTypes)
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

TEST_F(CSVEditorTableModelTest, ModifiedUntilSaved)
{
    // 編集すると未保存になり、保存すると戻ること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    model.insertColumn(1, "en");
    EXPECT_FALSE(model.isModified());
    EXPECT_TRUE(model.setData(model.index(0, 1), "Hi"));
    EXPECT_TRUE(model.isModified());
    EXPECT_TRUE(model.saveToFile(tempDir.filePath("Actors.csv")));
    EXPECT_FALSE(model.isModified());
}

TEST_F(CSVEditorTableModelTest, TranslatedRowCountIgnoresDisabledLanguage)
{
    // 有効な言語がすべて埋まった行だけを翻訳済みとして数えること
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([
        {"original": "A", "type": []},
        {"original": "B", "type": []}
    ])")));
    auto setting = std::make_shared<settings>();
    setting->languages.push_back({.languageName = "en", .enable = true});
    setting->languages.push_back({.languageName = "ja", .enable = false});
    model.setSettings(setting);
    model.insertColumn(1, "en");
    model.insertColumn(2, "ja");

    EXPECT_EQ(model.translatedRowCount(), std::make_pair(0, 2));
    EXPECT_TRUE(model.setData(model.index(0, 1), "a"));
    EXPECT_EQ(model.translatedRowCount(), std::make_pair(1, 2));
}

TEST_F(CSVEditorTableModelTest, EditedCellHidesStaleError)
{
    // 検証の後に編集したセルは、次の検証まで結果の色とアイコンを出さないこと
    langscore::CSVEditorTableModel model;
    ASSERT_TRUE(model.loadFromJsonFile(WriteJson(R"([{"original": "Hello", "type": []}])")));
    model.insertColumn(1, "en");
    auto runtimeData = std::make_shared<ComponentBase::RuntimeData>();
    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Error;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 1;
    info.language = "en";
    runtimeData->errors["Actors"].push_back(info);
    model.setRuntimeData(runtimeData);

    const auto cell = model.index(0, 1);
    EXPECT_TRUE(model.data(cell, Qt::BackgroundRole).isValid());
    EXPECT_TRUE(model.data(cell, Qt::DecorationRole).isValid());
    EXPECT_TRUE(model.data(cell, Qt::ToolTipRole).toString().contains("[Error]"));

    EXPECT_TRUE(model.setData(cell, "Hi"));
    EXPECT_FALSE(model.data(cell, Qt::BackgroundRole).isValid());
    EXPECT_FALSE(model.data(cell, Qt::DecorationRole).isValid());

    model.clearEditedSinceValidation();
    EXPECT_TRUE(model.data(cell, Qt::BackgroundRole).isValid());
}

TEST_F(CSVEditorTableModelTest, MergeTranslationWithLineBreak)
{
    // 改行（\n・\r\n）を含む原文の翻訳を保存し、読み直して戻せること
    for(const QByteArray original : {QByteArray(R"(a\nb)"), QByteArray(R"(a\r\nb)")})
    {
        const auto json = "[{\"original\": \"" + original + "\", \"type\": []}]";
        langscore::CSVEditorTableModel model;
        ASSERT_TRUE(model.loadFromJsonFile(WriteJson(json)));
        model.insertColumn(1, "en");
        ASSERT_TRUE(model.setData(model.index(0, 1), "Translated"));
        const auto csvPath = tempDir.filePath("Actors.csv");
        ASSERT_TRUE(model.saveToFile(csvPath));

        langscore::CSVEditorTableModel reopened;
        ASSERT_TRUE(reopened.loadFromJsonFile(WriteJson(json)));
        reopened.insertColumn(1, "en");
        ASSERT_TRUE(reopened.mergeTranslationsFromCsv(csvPath));
        EXPECT_EQ(reopened.getCell(0, 1).value_or(""), QString("Translated")) << original.toStdString();
    }
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
