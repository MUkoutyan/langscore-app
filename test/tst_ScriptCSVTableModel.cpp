#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include "ScriptCSVTableModel.h"
#include "EditorTableDefines.h"
#include "settings.h"

class ScriptCSVTableModelTest : public ::testing::Test
{
protected:
    QTemporaryDir tempDir;
    std::shared_ptr<settings> setting;

    void SetUp() override
    {
        // 有効言語 ja, en と、1行だけ持つ Plugin.js を用意する
        setting = std::make_shared<settings>();
        setting->projectType = settings::MZ;
        setting->languages = {
            settings::Language{"ja", {}, true},
            settings::Language{"en", {}, true},
        };
        settings::ScriptInfo info;
        info.fileName = "Plugin.js";
        info.scriptName = "Plugin";
        langscore::ScriptTextPosition line;
        line.type = langscore::ScriptTextPosition::Type::RowCol;
        line.originalText = "Hello";
        line.d = langscore::ScriptTextPosition::RowCol{1, 2};
        info.lines.push_back(line);
        setting->writeObj.scriptInfo.push_back(info);
    }

    void WriteEditJson(const QByteArray& content)
    {
        QFile file(tempDir.filePath("Scripts.lsjson"));
        if(file.open(QFile::WriteOnly) == false)
        {
            ADD_FAILURE() << "cannot create test file";
            return;
        }
        file.write(content);
    }

    QString LangCell(const ScriptCSVTableModel& model, int langIndex) const
    {
        return model.index(0, Columns::LangColumnStart + langIndex).data().toString();
    }
};

TEST_F(ScriptCSVTableModelTest, LoadRows)
{
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir.path());
    EXPECT_EQ(model.rowCount(), 1);
    EXPECT_EQ(model.columnCount(), Columns::LangColumnStart + 2);
    EXPECT_EQ(model.getOriginalText(0), QString("Hello"));
}

// 不具合の再発防止
TEST_F(ScriptCSVTableModelTest, LoadTranslations)
{
    // editing/Scripts.lsjson の translates に保存された翻訳が読み込まれること
    WriteEditJson(R"([{"original": "Hello", "file": "Plugin.js", "row": 1, "col": 2,
                      "translates": {"ja": "Konnichiwa", "en": "Hello!"}}])");
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir.path());
    EXPECT_EQ(LangCell(model, 0), QString("Konnichiwa"));
    EXPECT_EQ(LangCell(model, 1), QString("Hello!"));
}

TEST_F(ScriptCSVTableModelTest, LoadTranslationsWithMissingLanguage)
{
    // 一部の言語が欠けていても、他の言語の翻訳が別の列にずれないこと
    WriteEditJson(R"([{"original": "Hello", "file": "Plugin.js", "row": 1, "col": 2,
                      "translates": {"en": "Hello!"}}])");
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir.path());
    EXPECT_EQ(LangCell(model, 0), QString());
    EXPECT_EQ(LangCell(model, 1), QString("Hello!"));
}

TEST_F(ScriptCSVTableModelTest, SaveLoadRoundTrip)
{
    // 編集した翻訳を保存し、読み直しても失われないこと
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir.path());
    ASSERT_TRUE(model.setData(model.index(0, Columns::LangColumnStart + 1), "Hello!"));
    ASSERT_TRUE(model.saveToEditJsonFile(tempDir.filePath("Scripts.lsjson")));

    ScriptCSVTableModel reloaded;
    reloaded.setSettings(setting);
    reloaded.loadFromEditJSONWithSettings(tempDir.path());
    EXPECT_EQ(LangCell(reloaded, 1), QString("Hello!"));
}

TEST_F(ScriptCSVTableModelTest, IsLanguageColumn)
{
    // proxy の言語列非表示・翻訳済み行の非表示に必要な IsLanguageColumn に応答すること
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir.path());
    EXPECT_FALSE(model.headerData(Columns::OriginalText, Qt::Horizontal, DataType::IsLanguageColumn).toBool());
    EXPECT_TRUE(model.headerData(Columns::LangColumnStart, Qt::Horizontal, DataType::IsLanguageColumn).toBool());
}

TEST_F(ScriptCSVTableModelTest, SaveKeepsDisabledLanguage)
{
    // 無効にした言語の翻訳が、保存で Scripts.lsjson から消えないこと
    WriteEditJson(R"([{"original": "Hello", "file": "Plugin.js", "row": 1, "col": 2,
                      "translates": {"ja": "Konnichiwa", "en": "Hello!"}}])");
    setting->languages[1].enable = false;

    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir.path());
    ASSERT_TRUE(model.saveToEditJsonFile(tempDir.filePath("Scripts.lsjson")));

    QFile file(tempDir.filePath("Scripts.lsjson"));
    ASSERT_TRUE(file.open(QFile::ReadOnly));
    const auto rows = QJsonDocument::fromJson(file.readAll()).array();
    ASSERT_EQ(rows.size(), 1);
    const auto translates = rows[0].toObject()["translates"].toObject();
    EXPECT_EQ(translates["ja"].toString(), QString("Konnichiwa"));
    EXPECT_EQ(translates["en"].toString(), QString("Hello!"));
}
