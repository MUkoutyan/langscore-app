#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include "settings.h"
#include "CSVEditorTableModel.h"
#include "EditingTranslationExporter.h"

//-------------------------------------------------------------------
// 日本語などを含むフォルダで、実際の divisi.exe を使って
// 解析 → アプリで翻訳して保存 → 検証 → 書き出し が通ること。
// アプリをビルドしていない（bin/divisi.exe が無い）場合は飛ばす。
//-------------------------------------------------------------------
namespace
{
    void CopyDirectory(const QString& source, const QString& destination)
    {
        QDirIterator it(source, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while(it.hasNext())
        {
            const auto filePath = it.next();
            const auto target = destination + "/" + QDir(source).relativeFilePath(filePath);
            QDir().mkpath(QFileInfo(target).absolutePath());
            QFile::copy(filePath, target);
        }
    }

    struct ProcessResult
    {
        int exitCode = -1;
        QString output;
    };

    //アプリ（invoker）と同じ形の引数で divisi を実行する
    ProcessResult RunDivisi(const QString& configPath, const QString& option)
    {
        QProcess process;
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(LANGSCORE_DIVISI_PATH, {"-c", "\"" + configPath + "\"", option});
        ProcessResult result;
        if(process.waitForFinished(120000) == false) {
            result.output = "timeout";
            return result;
        }
        result.exitCode = process.exitCode();
        result.output = QString::fromUtf8(process.readAll());
        return result;
    }

    QString FindFile(const QString& directory, const QString& fileName)
    {
        QDirIterator it(directory, {fileName}, QDir::Files, QDirIterator::Subdirectories);
        return it.hasNext() ? it.next() : QString();
    }
}

class DivisiJapanesePathTest : public ::testing::TestWithParam<const char16_t*> {};

TEST_P(DivisiJapanesePathTest, AnalyzeValidateExport)
{
    if(QFile::exists(LANGSCORE_DIVISI_PATH) == false) {
        GTEST_SKIP() << "divisi.exe がありません。アプリをビルドしてから実行してください";
    }

    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath(QString::fromUtf16(GetParam()));
    CopyDirectory(QString(LANGSCORE_TEST_SOURCE_DIR) + "/Project1", gameDir);
    ASSERT_TRUE(QFile::exists(gameDir + "/Game.rvproj2"));

    settings setting;
    setting.setGameProjectPath(gameDir);
    ASSERT_EQ(setting.projectType, settings::VXAce);
    setting.defaultLanguage = "ja";
    setting.languages = {
        settings::Language{.languageName = "ja", .enable = true},
        settings::Language{.languageName = "en", .enable = true},
    };
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";
    QDir().mkpath(setting.langscoreProjectDirectory);
    setting.write(configPath);

    //解析
    const auto analyze = RunDivisi(configPath, "--analyze");
    ASSERT_EQ(analyze.exitCode, 0) << analyze.output.toStdString();
    const auto actorsJson = setting.analyzeDirectoryPath() + "/Actors.lsjson";
    ASSERT_TRUE(QFile::exists(actorsJson)) << analyze.output.toStdString();

    //アプリで翻訳して editing に保存する（MainCSVTable と同じ並び：original, 言語..., type）
    setting.load(configPath);
    langscore::CSVEditorTableModel model;
    model.setSettings(std::make_shared<settings>(setting));
    ASSERT_TRUE(model.loadFromJsonFile(actorsJson));
    model.insertColumn(1, "ja");
    model.insertColumn(2, "en");
    ASSERT_LT(0, model.rowCount());
    const QString translated(u"翻訳済み_한글");
    //原文が \r\n で改行している行（説明文）には、エディタと同じ \n で改行した訳を入れる
    int multiLineRow = -1;
    for(int row = 0; row < model.rowCount(); ++row)
    {
        const auto original = model.getCell(row, 0).value_or("");
        model.setData(model.index(row, 1), original);
        if(multiLineRow < 0 && original.contains("\r\n")) {
            multiLineRow = row;
            model.setData(model.index(row, 2), QString(u"一行目\n二行目"));
            continue;
        }
        model.setData(model.index(row, 2), translated);
    }
    ASSERT_LE(0, multiLineRow);
    const auto editingDir = setting.langscoreProjectDirectory + "/editing";
    QDir().mkpath(editingDir);
    ASSERT_TRUE(model.saveToFile(editingDir + "/Actors.csv"));

    //保存した翻訳を読み直せること
    {
        langscore::CSVEditorTableModel reopened;
        reopened.setSettings(std::make_shared<settings>(setting));
        ASSERT_TRUE(reopened.loadFromJsonFile(actorsJson));
        reopened.insertColumn(1, "ja");
        reopened.insertColumn(2, "en");
        ASSERT_TRUE(reopened.mergeTranslationsFromCsv(editingDir + "/Actors.csv"));
        QFile savedFile(editingDir + "/Actors.csv");
        savedFile.open(QFile::ReadOnly);
        const int plainRow = multiLineRow == 0 ? 1 : 0;
        EXPECT_EQ(reopened.getCell(plainRow, 2).value_or(""), translated) << QString::fromUtf8(savedFile.readAll()).toStdString();
        EXPECT_EQ(reopened.getCell(multiLineRow, 2).value_or(""), QString(u"一行目\n二行目"));
    }

    //検証（アプリの ValidationService と同じく、検証するファイルを設定に書いてから実行する）
    setting.validateObj.csvNameList = {editingDir + "/Actors.csv"};
    setting.write(configPath);
    const auto validate= RunDivisi(configPath, "--validate");
    EXPECT_EQ(validate.exitCode, 0) << validate.output.toStdString();
    //editing のCSVには言語の後に種類の列（type）があるが、言語の列とは数えないこと（NotEQLang = 4 が出ない）
    EXPECT_FALSE(validate.output.contains(R"("Summary":4)")) << validate.output.toStdString();

    //書き出し（アプリと同じく、divisiの書き出し後にアプリ側の翻訳を反映してからパッキングする）。
    //ゲームのフォルダの Data/Translate に、原文と翻訳が化けずに入り、訳の改行が原文と同じ \r\n になること
    setting.validateObj.csvNameList.clear();
    setting.write(configPath);
    const auto exportResult = RunDivisi(configPath, "--exportCSV");
    ASSERT_EQ(exportResult.exitCode, 0) << exportResult.output.toStdString();
    const auto exported = FindFile(setting.translateDirectoryPath(), "Actors.csv");
    ASSERT_FALSE(exported.isEmpty()) << exportResult.output.toStdString();
    EXPECT_TRUE(langscore::ApplyEditingTranslations(setting).contains(exported));
    QFile exportedFile(exported);
    ASSERT_TRUE(exportedFile.open(QFile::ReadOnly));
    const auto exportedText = QString::fromUtf8(exportedFile.readAll());
    EXPECT_TRUE(exportedText.contains(model.getCell(1, 0).value_or("?"))) << exported.toStdString();
    EXPECT_TRUE(exportedText.contains(translated)) << exported.toStdString();
    EXPECT_TRUE(exportedText.contains(QString(u"一行目\r\n二行目"))) << exported.toStdString();

    const auto packingResult = RunDivisi(configPath, "--packing");
    EXPECT_EQ(packingResult.exitCode, 0) << packingResult.output.toStdString();
}

//日本語と空白を含むフォルダ・CP932 に無い文字（ハングル）も含むフォルダ。
//ハングルのフォルダは、divisi が rvcnv.exe に渡すパスを ANSI に変換していたときに解析が失敗していたことの再発防止
INSTANTIATE_TEST_SUITE_P(
    FolderNames,
    DivisiJapanesePathTest,
    ::testing::Values(u"日本語 フォルダ/ゲーム", u"日本語 フォルダ/ゲーム_한글")
);

class DivisiReadOnlyTest : public ::testing::TestWithParam<bool> {};

TEST_P(DivisiReadOnlyTest, VXAceReadOnlyScriptsIsOverwritten)
{
    // VXAce のプラグインは Data/Scripts.rvdata2 に書き込まれる。
    // このファイルが読み取り専用でも、プラグインの書き出しが失敗せず、ファイルが書き換わること
    if(QFile::exists(LANGSCORE_DIVISI_PATH) == false) {
        GTEST_SKIP() << "divisi.exe がありません。アプリをビルドしてから実行してください";
    }

    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    CopyDirectory(QString(LANGSCORE_TEST_SOURCE_DIR) + "/Project1", gameDir);

    settings setting;
    setting.setGameProjectPath(gameDir);
    setting.defaultLanguage = "ja";
    setting.languages = {
        settings::Language{.languageName = "ja", .enable = true},
        settings::Language{.languageName = "en", .enable = true},
    };
    setting.writeObj.overwriteLangscore = true;
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";
    QDir().mkpath(setting.langscoreProjectDirectory);
    setting.write(configPath);
    const auto analyze = RunDivisi(configPath, "--analyze");
    ASSERT_EQ(analyze.exitCode, 0) << analyze.output.toStdString();

    const auto scriptsPath = gameDir + "/Data/Scripts.rvdata2";
    const auto before = QFileInfo(scriptsPath).size();
    if(GetParam()) {
        ASSERT_TRUE(QFile::setPermissions(scriptsPath, QFileDevice::ReadOwner | QFileDevice::ReadUser));
        ASSERT_FALSE(QFileInfo(scriptsPath).isWritable());
    }

    const auto update = RunDivisi(configPath, "--updatePlugin");
    //後片付けで消せるよう、結果に関係なく書き込みできるように戻す
    QFile::setPermissions(scriptsPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);

    EXPECT_EQ(update.exitCode, 0) << update.output.toStdString();
    EXPECT_NE(QFileInfo(scriptsPath).size(), before) << "Scripts.rvdata2 にプラグインが書き込まれていません: " << update.output.toStdString();
}

//false: 読み取り専用にしない場合（比べるため）
INSTANTIATE_TEST_SUITE_P(ReadOnly, DivisiReadOnlyTest, ::testing::Values(false, true));
