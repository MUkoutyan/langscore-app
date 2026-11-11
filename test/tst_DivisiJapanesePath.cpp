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
// アプリをビルドしておらず bin/divisi.exe が無い場合は飛ばす。
//-------------------------------------------------------------------
namespace
{
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

    //アプリで翻訳して editing に保存する。列は MainCSVTable と同じく original, 言語..., type の順
    setting.load(configPath);
    langscore::CSVEditorTableModel model;
    model.setSettings(std::make_shared<settings>(setting));
    ASSERT_TRUE(model.loadFromJsonFile(actorsJson));
    model.insertColumn(1, "ja");
    model.insertColumn(2, "en");
    ASSERT_LT(0, model.rowCount());
    const QString translated(u"翻訳済み_한글");
    //説明文など原文が \r\n で改行している行には、エディタと同じ \n で改行した翻訳文を入れる
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
        ASSERT_TRUE(savedFile.open(QFile::ReadOnly));
        const int plainRow = multiLineRow == 0 ? 1 : 0;
        EXPECT_EQ(reopened.getCell(plainRow, 2).value_or(""), translated) << QString::fromUtf8(savedFile.readAll()).toStdString();
        EXPECT_EQ(reopened.getCell(multiLineRow, 2).value_or(""), QString(u"一行目\n二行目"));
    }

    //検証。アプリの ValidationService と同じく、検証するファイルを設定に書いてから実行する
    setting.validateObj.csvNameList = {editingDir + "/Actors.csv"};
    setting.write(configPath);
    const auto validate= RunDivisi(configPath, "--validate");
    EXPECT_EQ(validate.exitCode, 0) << validate.output.toStdString();
    //editing の CSV には言語の後に type 列があるが、言語の列として数えず NotEQLang = 4 が出ないこと
    EXPECT_FALSE(validate.output.contains(R"("Summary":4)")) << validate.output.toStdString();

    //書き出し。アプリと同じく、divisi の書き出し後にアプリ側の翻訳文を反映してからパッキングする。
    //ゲームのフォルダの Data/Translate に、原文と翻訳文が化けずに入り、翻訳文の改行が原文と同じ \r\n になること
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

//日本語と空白を含むフォルダと、ハングルなど CP932 に無い文字も含むフォルダ。
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
    //後片付けで削除できるよう、結果に関係なく書き込み可能に戻す
    QFile::setPermissions(scriptsPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);

    EXPECT_EQ(update.exitCode, 0) << update.output.toStdString();
    EXPECT_NE(QFileInfo(scriptsPath).size(), before) << "Scripts.rvdata2 にプラグインが書き込まれていません: " << update.output.toStdString();
}

//false: 比較用の、読み取り専用にしない場合
INSTANTIATE_TEST_SUITE_P(ReadOnly, DivisiReadOnlyTest, ::testing::Values(false, true));

class DivisiMvReadOnlyTest : public ::testing::TestWithParam<bool> {};

TEST_P(DivisiMvReadOnlyTest, ReadOnlyPluginsJsRegistersLangscore)
{
    // MV・MZ では Langscore を js/plugins.js に登録しないとゲームで使われない。
    // plugins.js が読み取り専用でも登録されること。以前は書き込みに失敗しても成功として終わっていた
    if(QFile::exists(LANGSCORE_DIVISI_PATH) == false) {
        GTEST_SKIP() << "divisi.exe がありません。アプリをビルドしてから実行してください";
    }

    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    QDir().mkpath(gameDir + "/js/plugins");
    QDir().mkpath(gameDir + "/data");
    const auto pluginsPath = gameDir + "/js/plugins.js";
    {
        QFile project(gameDir + "/Game.rpgproject");
        ASSERT_TRUE(project.open(QFile::WriteOnly));
        project.write("RPGMV 1.6.2");
        QFile plugins(pluginsPath);
        ASSERT_TRUE(plugins.open(QFile::WriteOnly));
        plugins.write("// Generated by RPG Maker.\n// Do not edit this file directly.\nvar $plugins =\n[\n];\n");
    }

    settings setting;
    setting.setGameProjectPath(gameDir);
    ASSERT_EQ(setting.projectType, settings::MV);
    setting.defaultLanguage = "ja";
    setting.languages = {
        settings::Language{.languageName = "ja", .enable = true},
        settings::Language{.languageName = "en", .enable = true},
    };
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";
    QDir().mkpath(setting.langscoreProjectDirectory);
    setting.write(configPath);

    if(GetParam()) {
        ASSERT_TRUE(QFile::setPermissions(pluginsPath, QFileDevice::ReadOwner | QFileDevice::ReadUser));
        ASSERT_FALSE(QFileInfo(pluginsPath).isWritable());
    }
    const auto update = RunDivisi(configPath, "--updatePlugin");
    //後片付けで削除できるよう、結果に関係なく書き込み可能に戻す
    QFile::setPermissions(pluginsPath, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ReadUser | QFileDevice::WriteUser);

    EXPECT_EQ(update.exitCode, 0) << update.output.toStdString();
    QFile plugins(pluginsPath);
    ASSERT_TRUE(plugins.open(QFile::ReadOnly));
    EXPECT_TRUE(plugins.readAll().contains(R"("name":"Langscore")")) << "plugins.js に Langscore が登録されていません: " << update.output.toStdString();
}

//false: 比較用の、読み取り専用にしない場合
INSTANTIATE_TEST_SUITE_P(ReadOnly, DivisiMvReadOnlyTest, ::testing::Values(false, true));

TEST(DivisiVXAceScriptOrderTest, CustomScriptIsPlacedBelowLangscore)
{
    // langscore_custom は langscore の Langscore モジュールに定義を追加するため、langscore より下に無いと
    // ゲームの起動時に NameError になる。プラグインの追加で langscore のすぐ下に置かれること。
    // 以前の divisi が langscore より上に置いたゲームも、プラグインの更新で並べ直されること
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
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";
    QDir().mkpath(setting.langscoreProjectDirectory);
    setting.write(configPath);
    const auto analyze = RunDivisi(configPath, "--analyze");
    ASSERT_EQ(analyze.exitCode, 0) << analyze.output.toStdString();

    //_list.csv のスクリプト名の並び。divisi はプラグインの追加・更新時に、ここから Scripts.rvdata2 を作成する
    const auto listPath = setting.analyzeDirectoryPath() + "/Scripts/_list.csv";
    const auto ReadScriptLines = [&listPath]() {
        QFile file(listPath);
        if(file.open(QFile::ReadOnly) == false) { return QList<QByteArray>(); }
        auto lines = file.readAll().split('\n');
        for(auto& line : lines) { line = line.trimmed(); }
        return lines;
    };
    const auto IndexOfScript = [](const QList<QByteArray>& lines, const QByteArray& name) {
        const auto found = std::ranges::find_if(lines, [&name](const QByteArray& line) { return line.endsWith("," + name); });
        return found == lines.end() ? -1 : int(std::distance(lines.begin(), found));
    };
    const auto ExpectCustomBelowLangscore = [&](const ProcessResult& update) {
        ASSERT_EQ(update.exitCode, 0) << update.output.toStdString();
        const auto lines = ReadScriptLines();
        const auto langscore = IndexOfScript(lines, "langscore");
        ASSERT_LE(0, langscore);
        EXPECT_EQ(IndexOfScript(lines, "langscore_custom"), langscore + 1) << lines.join('\n').toStdString();
    };

    ExpectCustomBelowLangscore(RunDivisi(configPath, "--updatePlugin"));

    //以前の divisi が作成した、langscore_custom が langscore より上にある並びにしてから更新する
    {
        auto lines = ReadScriptLines();
        const auto custom = IndexOfScript(lines, "langscore_custom");
        ASSERT_LE(0, custom);
        const auto customLine = lines.takeAt(custom);
        lines.insert(IndexOfScript(lines, "langscore"), customLine);
        QFile file(listPath);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        file.write(lines.join("\r\n"));
    }
    ExpectCustomBelowLangscore(RunDivisi(configPath, "--updatePlugin"));
}
