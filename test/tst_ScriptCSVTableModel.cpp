#include <QTest>
#include <QTemporaryDir>
#include <QFile>
#include "ScriptCSVTableModel.h"
#include "EditorTableDefines.h"
#include "settings.h"

class TestScriptCSVTableModel : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void testLoadRows();

    // 不具合の再発防止
    void testLoadTranslations();
    void testLoadTranslationsWithMissingLanguage();
    void testSaveLoadRoundTrip();
    void testIsLanguageColumn();

private:
    std::unique_ptr<QTemporaryDir> tempDir;
    std::shared_ptr<settings> setting;
    void writeEditJson(const QByteArray& content);
    QString langCell(const ScriptCSVTableModel& model, int langIndex) const;
};

void TestScriptCSVTableModel::init()
{
    tempDir = std::make_unique<QTemporaryDir>();

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

void TestScriptCSVTableModel::writeEditJson(const QByteArray& content)
{
    QFile file(tempDir->filePath("Scripts.lsjson"));
    if(file.open(QFile::WriteOnly) == false){ qFatal("cannot create test file"); }
    file.write(content);
}

QString TestScriptCSVTableModel::langCell(const ScriptCSVTableModel& model, int langIndex) const
{
    return model.index(0, Columns::LangColumnStart + langIndex).data().toString();
}

void TestScriptCSVTableModel::testLoadRows()
{
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir->path());
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.columnCount(), Columns::LangColumnStart + 2);
    QCOMPARE(model.getOriginalText(0), QString("Hello"));
}

void TestScriptCSVTableModel::testLoadTranslations()
{
    // editing/Scripts.lsjson の translates に保存された翻訳が読み込まれること
    writeEditJson(R"([{"original": "Hello", "file": "Plugin.js", "row": 1, "col": 2,
                      "translates": {"ja": "Konnichiwa", "en": "Hello!"}}])");
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir->path());
    QCOMPARE(langCell(model, 0), QString("Konnichiwa"));
    QCOMPARE(langCell(model, 1), QString("Hello!"));
}

void TestScriptCSVTableModel::testLoadTranslationsWithMissingLanguage()
{
    // 一部の言語が欠けていても、他の言語の翻訳が別の列にずれないこと
    writeEditJson(R"([{"original": "Hello", "file": "Plugin.js", "row": 1, "col": 2,
                      "translates": {"en": "Hello!"}}])");
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir->path());
    QCOMPARE(langCell(model, 0), QString());
    QCOMPARE(langCell(model, 1), QString("Hello!"));
}

void TestScriptCSVTableModel::testSaveLoadRoundTrip()
{
    // 編集した翻訳を保存し、読み直しても失われないこと
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir->path());
    QVERIFY(model.setData(model.index(0, Columns::LangColumnStart + 1), "Hello!"));
    QVERIFY(model.saveToEditJsonFile(tempDir->filePath("Scripts.lsjson")));

    ScriptCSVTableModel reloaded;
    reloaded.setSettings(setting);
    reloaded.loadFromEditJSONWithSettings(tempDir->path());
    QCOMPARE(langCell(reloaded, 1), QString("Hello!"));
}

void TestScriptCSVTableModel::testIsLanguageColumn()
{
    // proxy の言語列非表示・翻訳済み行の非表示に必要な IsLanguageColumn に応答すること
    ScriptCSVTableModel model;
    model.setSettings(setting);
    model.loadFromEditJSONWithSettings(tempDir->path());
    QCOMPARE(model.headerData(Columns::OriginalText, Qt::Horizontal, DataType::IsLanguageColumn).toBool(), false);
    QCOMPARE(model.headerData(Columns::LangColumnStart, Qt::Horizontal, DataType::IsLanguageColumn).toBool(), true);
}

QTEST_MAIN(TestScriptCSVTableModel)
#include "tst_ScriptCSVTableModel.moc"
