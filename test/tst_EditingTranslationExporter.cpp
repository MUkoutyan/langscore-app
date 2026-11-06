#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "settings.h"
#include "FastCSVContainer.h"
#include "EditingTranslationExporter.h"
#include "ScriptCSVTableModel.h"
#include "EditorTableDefines.h"

//-------------------------------------------------------------------
// 書き出したCSVに editing の翻訳を反映する処理と、改行の扱い
//-------------------------------------------------------------------
namespace
{
    void WriteBytes(const QString& path, const QByteArray& bytes)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        file.write(bytes);
    }

    //MSVC は /utf-8 無しでは char の文字列を ANSI にするため、UTF-16 から UTF-8 にする
    QByteArray Utf8(const char16_t* text)
    {
        return QString::fromUtf16(text).toUtf8();
    }

    //divisi が書き出すCSVの BOM（fromUtf16 は先頭の BOM を取り除くため別に足す）
    const QByteArray Bom("\xEF\xBB\xBF");

    QByteArray ReadBytes(const QString& path)
    {
        QFile file(path);
        if(file.open(QFile::ReadOnly) == false) { return {}; }
        return file.readAll();
    }

    //VXAce・既定言語 ja・ja と en を使うプロジェクト
    settings MakeSetting(const QString& langscoreProjectDirectory)
    {
        settings setting;
        setting.langscoreProjectDirectory = langscoreProjectDirectory;
        setting.projectType = settings::VXAce;
        setting.defaultLanguage = "ja";
        setting.languages = {
            settings::Language{.languageName = "ja", .enable = true},
            settings::Language{.languageName = "en", .enable = true},
        };
        setting.writeObj.exportDirectory = "Data/Translate";
        setting.writeObj.writeMode = 2;
        return setting;
    }

    //アプリが保存する editing の CSV（セル内の改行は \n）
    const QByteArray EditingCsv = Utf8(
        u"original,ja,en,type\n"
        u"\"説明\r\n2行目\",,\"Line1\nLine2\",description\n"
        u"名前,,,name\n");

    //divisi が書き出す CSV（BOM 付き、原文は \r\n のまま）
    const QByteArray ExportedCsv = Bom + Utf8(
        u"original,ja,en\n"
        u"\"説明\r\n2行目\",,old\n"
        u"名前,,keep");
}

TEST(EditingTranslationExporter, LineBreakFollowsOriginal)
{
    //原文に改行があれば原文に合わせ、無ければツクールの改行にする
    EXPECT_EQ(langscore::ToGameLineBreak("a\nb", "x\r\ny", false), QString("a\r\nb"));
    EXPECT_EQ(langscore::ToGameLineBreak("a\r\nb", "x\ny", true), QString("a\nb"));
    EXPECT_EQ(langscore::ToGameLineBreak("a\nb", "x", true), QString("a\r\nb"));
    EXPECT_EQ(langscore::ToGameLineBreak("a\r\nb", "x", false), QString("a\nb"));
}

TEST(EditingTranslationExporter, KeepExistingWhenEditingIsEmpty)
{
    QTemporaryDir tempDir;
    const auto setting = MakeSetting(tempDir.path());
    WriteBytes(tempDir.filePath("editing/Actors.csv"), EditingCsv);
    const auto exportPath = tempDir.filePath("Data/Translate/Actors.csv");
    WriteBytes(exportPath, ExportedCsv);

    const auto updated = langscore::ApplyEditingTranslations(setting);
    ASSERT_EQ(updated.size(), 1);

    //原文はそのまま、訳は原文と同じ \r\n、editing で空の訳は書き出し済みの訳を残す
    const QByteArray expected = Bom + Utf8(
        u"original,ja,en\n"
        u"\"説明\r\n2行目\",,\"Line1\r\nLine2\"\n"
        u"名前,,keep");
    EXPECT_EQ(ReadBytes(exportPath), expected);
}

TEST(EditingTranslationExporter, OverwriteModeReplacesWithEditing)
{
    QTemporaryDir tempDir;
    auto setting = MakeSetting(tempDir.path());
    setting.writeObj.writeMode = 0;
    WriteBytes(tempDir.filePath("editing/Actors.csv"), EditingCsv);
    const auto exportPath = tempDir.filePath("Data/Translate/Actors.csv");
    WriteBytes(exportPath, ExportedCsv);

    langscore::ApplyEditingTranslations(setting);

    //新しい内容で上書きする場合は、editing で空の訳も空にする
    const QByteArray expected = Bom + Utf8(
        u"original,ja,en\n"
        u"\"説明\r\n2行目\",,\"Line1\r\nLine2\"\n"
        u"名前,,");
    EXPECT_EQ(ReadBytes(exportPath), expected);
}

TEST(EditingTranslationExporter, LanguagePatchFolders)
{
    //言語ごとに書き出す場合は、言語ごとのフォルダのCSVに反映する
    QTemporaryDir tempDir;
    auto setting = MakeSetting(tempDir.path());
    setting.projectType = settings::MZ;
    setting.writeObj.enableLanguagePatch = true;
    WriteBytes(tempDir.filePath("editing/Actors.csv"), EditingCsv);
    const auto exportPath = tempDir.filePath("Data/Translate/en/Actors.csv");
    WriteBytes(exportPath, Bom + Utf8(u"original,en\n\"説明\r\n2行目\",\n名前,"));

    const auto updated = langscore::ApplyEditingTranslations(setting);
    ASSERT_EQ(updated.size(), 1);
    EXPECT_TRUE(ReadBytes(exportPath).contains("\"Line1\r\nLine2\""));
}

TEST(FastCSVContainer, LineBreakInCellIsSavedOnceAndReadAsLF)
{
    //セル内の \r\n が \r\r\n にならずに保存され、読むと \n にそろうこと。
    //以前の保存（Text モード）で \r\r\n になったファイルも \n で読めること
    QTemporaryDir tempDir;
    const auto path = tempDir.filePath("lb.csv");
    langscore::FastCSVContainer csv;
    csv.loadFromCsvData({{"original", "en"}, {"a\r\nb", "c\nd"}});
    ASSERT_TRUE(csv.saveToFile(path));
    EXPECT_FALSE(ReadBytes(path).contains("\r\r\n"));

    langscore::FastCSVContainer reopened;
    ASSERT_TRUE(reopened.loadFromFile(path));
    EXPECT_EQ(reopened.at(0, 0), QString("a\nb"));
    EXPECT_EQ(reopened.at(0, 1), QString("c\nd"));

    WriteBytes(path, "original,en\r\n\"a\r\r\nb\",x\r\n");
    ASSERT_TRUE(reopened.loadFromFile(path));
    EXPECT_EQ(reopened.at(0, 0), QString("a\nb"));
    EXPECT_EQ(reopened.at(0, 1), QString("x"));
}

// 不具合の再発防止：スクリプトの表で入力した翻訳が、書き出した Scripts.csv に入らなかった
TEST(EditingTranslationExporter, ScriptTranslationsAreApplied)
{
    QTemporaryDir tempDir;
    auto setting = std::make_shared<settings>(MakeSetting(tempDir.path()));
    setting->projectType = settings::MZ;
    //同じ原文が2つのスクリプトにあっても、訳は1行にまとまる。A.js にはプラグインのパラメータもある
    for(const auto& fileName : {QString("A.js"), QString("B.js")})
    {
        settings::ScriptInfo info;
        info.fileName = fileName;
        info.scriptName = fileName;
        langscore::ScriptTextPosition line;
        line.type = langscore::ScriptTextPosition::Type::RowCol;
        line.originalText = "Hello";
        line.d = langscore::ScriptTextPosition::RowCol{1, 2};
        info.lines.push_back(line);
        if(fileName == "A.js")
        {
            langscore::ScriptTextPosition param;
            param.type = langscore::ScriptTextPosition::Type::Argument;
            param.originalText = QString(u"外に出る");
            param.d = langscore::ScriptTextPosition::ScriptArg{"Exit Command"};
            info.lines.push_back(param);
        }
        setting->writeObj.scriptInfo.push_back(info);
    }

    ScriptCSVTableModel model;
    model.setSettings(setting);
    const auto editingFolder = tempDir.filePath("editing");
    QDir().mkpath(editingFolder);
    model.loadFromEditJSONWithSettings(editingFolder);
    ASSERT_EQ(model.rowCount(), 3);
    //言語は ja, en の順。B.js の "Hello" とパラメータの en に入力する
    for(int row = 0; row < model.rowCount(); ++row)
    {
        const auto original = model.getOriginalText(row);
        if(original == "Hello" && model.getScriptFileName(row) == "B.js") {
            ASSERT_TRUE(model.setData(model.index(row, Columns::LangColumnStart + 1), "Translated", Qt::EditRole));
        }
        else if(original == QString(u"外に出る")) {
            ASSERT_TRUE(model.setData(model.index(row, Columns::LangColumnStart + 1), "Go outside", Qt::EditRole));
        }
    }
    ASSERT_TRUE(model.saveToFile(editingFolder + "/Scripts.lsjson"));

    //divisi の Scripts.csv は、1列目が「ファイル名:行:列」か「ファイル名:パラメータ名」のキーで、プラグインはこのキーで訳を引く
    const auto exportPath = tempDir.filePath("Data/Translate/Scripts.csv");
    WriteBytes(exportPath, Bom + QString(u"original,ja,en\nA:1:2,Hello,\nA:Exit Command,外に出る,\nB:1:2,Hello,").toUtf8());
    langscore::ApplyEditingTranslations(*setting);
    EXPECT_EQ(QString::fromUtf8(ReadBytes(exportPath)), QString::fromUtf8(Bom) + QString(u"original,ja,en\nA:1:2,Hello,Translated\nA:Exit Command,外に出る,Go outside\nB:1:2,Hello,Translated"));
}

TEST(EditingTranslationExporter, RemovePackedCsvFiles)
{
    //パッキングできたCSVだけを削除し、.rvdata2 が無いCSVは残す
    QTemporaryDir tempDir;
    auto setting = MakeSetting(tempDir.path());
    setting.gameProjectPath = tempDir.path();
    WriteBytes(tempDir.filePath("Data/Translate/Actors.csv"), "original,ja,en");
    WriteBytes(tempDir.filePath("Data/Translate/Actors.rvdata2"), "packed");
    WriteBytes(tempDir.filePath("Data/Translate/Items.csv"), "original,ja,en");

    const auto removed = langscore::RemovePackedCsvFiles(setting);
    EXPECT_EQ(removed.size(), 1);
    EXPECT_FALSE(QFile::exists(tempDir.filePath("Data/Translate/Actors.csv")));
    EXPECT_TRUE(QFile::exists(tempDir.filePath("Data/Translate/Actors.rvdata2")));
    EXPECT_TRUE(QFile::exists(tempDir.filePath("Data/Translate/Items.csv")));
}

TEST(EditingTranslationExporter, RemovePackedCsvFilesLanguagePatch)
{
    //パッチモードでは、言語ごとのフォルダにパッキングしたものを見る
    QTemporaryDir tempDir;
    auto setting = MakeSetting(tempDir.path());
    setting.gameProjectPath = tempDir.path();
    setting.writeObj.enableLanguagePatch = true;
    WriteBytes(tempDir.filePath("Data/Translate/en/Actors.csv"), "original,en");
    WriteBytes(tempDir.filePath("Data/Translate/en/Actors.rvdata2"), "packed");
    WriteBytes(tempDir.filePath("Data/Translate/ja/Actors.csv"), "original,ja");

    const auto removed = langscore::RemovePackedCsvFiles(setting);
    EXPECT_EQ(removed.size(), 1);
    EXPECT_FALSE(QFile::exists(tempDir.filePath("Data/Translate/en/Actors.csv")));
    EXPECT_TRUE(QFile::exists(tempDir.filePath("Data/Translate/ja/Actors.csv")));
}
