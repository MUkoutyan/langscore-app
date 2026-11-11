#include "test_common.h"
#include <QTemporaryDir>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include "settings.h"
#include "CSVEditorTableModel.h"
#include "EditingTranslationExporter.h"
#include "dialog/AnalyzeDialog.h"
#include "dialog/RPGMakerPathDialog.h"

//-------------------------------------------------------------------
// ツクールごとに、ゲームを初めて Langscore で開いてからテストプレイまでを通しで確認する。
//   1. 解析画面でゲームを開き「翻訳を開始」する。解析 → プラグインの追加 → 翻訳CSVの書き出しの順に行う
//   2. 言語を有効にしてプラグインを更新し、Actors を翻訳して書き出す
//   3. テストプレイと同じコマンドでゲームを起動し、タイトル → ニューゲーム → マップまで進める
//      test/game_boot のスクリプトをゲームに追加し、各言語のアクター名を出力させてから終了させる
// プラグインを追加したゲームがエラー無く動作し、翻訳文がゲームに反映されることを確認する。
//
// 必要なもの。無ければテストを飛ばす
//   - アプリのビルドで生成される bin/divisi.exe
//   - MV・MZ：リポジトリと同階層、または環境変数 LANGSCORE_TEST_DATA にある langscore-divisi-test-data
//   - MV・MZ：環境変数 LANGSCORE_RPGMAKER_MV / LANGSCORE_RPGMAKER_MZ、
//     またはアプリの「ツクールの場所」の設定で指定したツクールのインストール先
// ゲームのウィンドウが開くため、デスクトップで実行すること。
//-------------------------------------------------------------------
namespace
{
    constexpr int GameTimeoutMs = 180000;
    const QString BootResultName = "ls_boot_result.txt";
    const QString TranslatedPrefix = "LS_";

    struct FirstRunTestComponent : public ComponentBase
    {
        using ComponentBase::setting;
    };

    QString TestDataRoot()
    {
        const auto env = qEnvironmentVariable("LANGSCORE_TEST_DATA");
        if(env.isEmpty() == false) { return env; }
        return QString(LANGSCORE_TEST_SOURCE_DIR) + "/../../langscore-divisi-test-data";
    }

    //Langscore を一度も使っていないゲーム
    QString SourceProject(settings::ProjectType type)
    {
        switch(type)
        {
        case settings::VXAce: return QString(LANGSCORE_TEST_SOURCE_DIR) + "/Project1";
        case settings::MV:    return TestDataRoot() + "/data/LangscoreTestMV";
        case settings::MZ:    return TestDataRoot() + "/data/LangscoreTestMZ";
        default:              return {};
        }
    }

    //環境変数が無ければ、アプリに設定したツクールの場所を使う
    QString RPGMakerPath(settings::ProjectType type)
    {
        if(type == settings::VXAce) { return {}; }
        const auto env = qEnvironmentVariable(type == settings::MV ? "LANGSCORE_RPGMAKER_MV" : "LANGSCORE_RPGMAKER_MZ");
        if(env.isEmpty() == false) { return env; }
        const QSettings appSettings(QFileInfo(LANGSCORE_DIVISI_PATH).absolutePath() + "/../settings.ini", QSettings::IniFormat);
        return appSettings.value(type == settings::MV ? "RPGMakerPath/MV" : "RPGMakerPath/MZ").toString();
    }

    //解析画面の invoker が使う exe の隣の bin に、アプリと一緒に置かれる divisi 一式をテストの間だけ置く
    class ScopedRealDivisi
    {
    public:
        ScopedRealDivisi()
            : binDir(QCoreApplication::applicationDirPath() + "/bin")
            , existed(QDir(binDir).exists())
            , copied(CopyDirectory(QFileInfo(LANGSCORE_DIVISI_PATH).absolutePath(), binDir))
        {}

        ~ScopedRealDivisi()
        {
            if(existed == false) {
                QDir(binDir).removeRecursively();
                return;
            }
            for(const auto& path : copied) { QFile::remove(path); }
        }

        ScopedRealDivisi(const ScopedRealDivisi&) = delete;
        ScopedRealDivisi& operator=(const ScopedRealDivisi&) = delete;

    private:
        QString binDir;
        bool existed;
        QStringList copied;
    };

    QString ReadText(const QString& path)
    {
        QFile file(path);
        if(file.open(QFile::ReadOnly) == false) { return {}; }
        return QString::fromUtf8(file.readAll());
    }

    bool WriteText(const QString& path, const QByteArray& content)
    {
        QFile file(path);
        if(file.open(QFile::WriteOnly) == false) { return false; }
        return file.write(content) == content.size();
    }

    //MV・MZ の plugins.js の $plugins を読む
    QJsonArray ReadPlugins(const QString& gameDir)
    {
        const auto text = ReadText(gameDir + "/js/plugins.js");
        const auto start = text.indexOf('[');
        const auto end = text.lastIndexOf(']');
        if(start < 0 || end < start) { return {}; }
        return QJsonDocument::fromJson(text.mid(start, end - start + 1).toUtf8()).array();
    }

    //MV・MZ：チェック用のプラグインを、Langscore.js より先に読まれるよう plugins.js の先頭に登録する
    QString InjectBootCheckMvMz(const QString& gameDir)
    {
        const auto pluginPath = gameDir + "/js/plugins/LangscoreBootCheck.js";
        QFile::remove(pluginPath);
        if(QFile::copy(QString(LANGSCORE_TEST_SOURCE_DIR) + "/game_boot/LangscoreBootCheck.js", pluginPath) == false) {
            return "LangscoreBootCheck.js をコピーできません";
        }
        auto text = ReadText(gameDir + "/js/plugins.js");
        const auto start = text.indexOf('[');
        if(start < 0) { return "plugins.js に $plugins がありません"; }
        text.insert(start + 1, R"({"name":"LangscoreBootCheck","status":true,"description":"","parameters":{}},)");
        if(WriteText(gameDir + "/js/plugins.js", text.toUtf8()) == false) { return "plugins.js に書き込めません"; }
        return {};
    }

    //VXAce：Scripts.rvdata2 を rvcnv で展開し、Main の直前にチェック用のスクリプトを追加して再圧縮する
    QString InjectBootCheckVXAce(const QString& gameDir, const QString& workDir)
    {
        const auto rvcnv = QFileInfo(LANGSCORE_DIVISI_PATH).absolutePath() + "/rvcnv.exe";
        //rvcnv の圧縮は「-i で渡したフォルダ名_langscore/analyze/Scripts」から読む
        const auto injectDir = workDir + "/Inject";
        const auto analyzeDir = injectDir + "_langscore/analyze";
        QDir().mkpath(analyzeDir);
        QDir().mkpath(injectDir + "/Data");

        const auto extract = RunProcess(rvcnv, {"-i", gameDir, "-o", analyzeDir});
        if(extract.exitCode != 0) { return "rvcnv で展開できません: " + extract.output; }

        const auto listPath = analyzeDir + "/Scripts/_list.csv";
        QFile listFile(listPath);
        if(listFile.open(QFile::ReadOnly) == false) { return "_list.csv がありません: " + extract.output; }
        auto lines = listFile.readAll().split('\n');
        listFile.close();
        const auto mainLine = std::ranges::find_if(lines, [](const QByteArray& line) { return line.trimmed().endsWith(",Main"); });
        if(mainLine == lines.end()) { return "_list.csv に Main がありません"; }
        const QByteArray lineEnd = mainLine->endsWith('\r') ? "\r" : "";
        lines.insert(mainLine, "999001,LsBootCheck" + lineEnd);
        if(WriteText(listPath, lines.join('\n')) == false) { return "_list.csv に書き込めません"; }
        if(QFile::copy(QString(LANGSCORE_TEST_SOURCE_DIR) + "/game_boot/ls_boot_check.rb", analyzeDir + "/Scripts/999001.rb") == false) {
            return "ls_boot_check.rb をコピーできません";
        }

        const auto compress = RunProcess(rvcnv, {"-i", injectDir, "-c"});
        const auto built = injectDir + "/Data/Scripts.rvdata2";
        if(compress.exitCode != 0 || QFile::exists(built) == false) { return "rvcnv で圧縮できません: " + compress.output; }
        QFile::remove(gameDir + "/Data/Scripts.rvdata2");
        if(QFile::copy(built, gameDir + "/Data/Scripts.rvdata2") == false) { return "Scripts.rvdata2 を置き換えられません"; }
        return {};
    }
}

class FirstRunGamePlayTest : public ::testing::TestWithParam<settings::ProjectType> {};

TEST_P(FirstRunGamePlayTest, PluginAddedGameRunsWithTranslation)
{
    const auto type = GetParam();
    if(QFile::exists(LANGSCORE_DIVISI_PATH) == false) {
        GTEST_SKIP() << "divisi.exe がありません。アプリをビルドしてから実行してください";
    }
    const auto source = SourceProject(type);
    if(QFileInfo::exists(source) == false) {
        GTEST_SKIP() << "テスト用のゲームがありません: " << source.toStdString();
    }
    const auto rpgMakerPath = RPGMakerPath(type);
    if(type != settings::VXAce && QFile::exists(RPGMakerPathDialog::testPlayCommand(type, "", rpgMakerPath).first) == false) {
        GTEST_SKIP() << "ツクールの場所が分かりません。環境変数 LANGSCORE_RPGMAKER_MV / LANGSCORE_RPGMAKER_MZ で指定してください";
    }

    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    CopyDirectory(source, gameDir);

    //---- 1. 解析画面で「翻訳を開始」する ----
    FirstRunTestComponent component;
    auto& setting = *component.setting;
    {
        ScopedRealDivisi divisi;
        AnalyzeDialog dialog(&component);
        dialog.openFile(gameDir);
        ASSERT_EQ(setting.projectType, type);
        auto* startButton = dialog.findChild<QPushButton*>("analyzeButton");
        auto* log = dialog.findChild<QPlainTextEdit*>("invokeLog");
        ASSERT_NE(startButton, nullptr);
        ASSERT_NE(log, nullptr);
        ASSERT_TRUE(startButton->isEnabled());

        bool started = false;
        QObject::connect(&dialog, &AnalyzeDialog::translationStarted, [&started]() { started = true; });
        startButton->click();
        //失敗するとボタンが押せる状態に戻る
        ASSERT_TRUE(WaitUntil([&]() { return started || startButton->isEnabled(); }, 600000)) << log->toPlainText().toStdString();
        ASSERT_TRUE(started) << log->toPlainText().toStdString();
    }
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";

    //ゲームにプラグインと翻訳ファイルが追加されている
    if(type == settings::VXAce)
    {
        EXPECT_FALSE(QDir(setting.translateDirectoryPath()).entryList({"*.rvdata2"}, QDir::Files).isEmpty());
        //パッキングしたCSVは残さない
        EXPECT_TRUE(QDir(setting.translateDirectoryPath()).entryList({"*.csv"}, QDir::Files).isEmpty());
    }
    else
    {
        const auto plugin = ReadText(gameDir + "/js/plugins/Langscore.js");
        ASSERT_FALSE(plugin.isEmpty());
        EXPECT_FALSE(plugin.contains("%{")) << "Langscore.js に置き換えられていないテンプレートの記述があります";
        const auto plugins = ReadPlugins(gameDir);
        const auto registered = std::ranges::any_of(plugins, [](const QJsonValue& value) {
            return value["name"].toString() == "Langscore" && value["status"].toBool();
        });
        EXPECT_TRUE(registered) << "plugins.js に Langscore が有効な状態で登録されていません: " << ReadText(gameDir + "/js/plugins.js").toStdString();
        EXPECT_FALSE(QDir(setting.translateDirectoryPath()).entryList({"*.csv"}, QDir::Files).isEmpty());
    }

    //---- 2. 言語を有効にしてプラグインを更新し、Actors を翻訳して書き出す ----
    const QString targetLanguage = setting.defaultLanguage == "en" ? "ja" : "en";
    {
        auto language = std::ranges::find_if(setting.languages, [&targetLanguage](const auto& lang) { return lang.languageName == targetLanguage; });
        ASSERT_NE(language, setting.languages.end());
        language->enable = true;
        //プラグインの更新ダイアログで「Langscore を上書きする」を選んだ状態
        setting.writeObj.overwriteLangscore = true;
        setting.write(configPath);
        const auto update = RunDivisi(configPath, "--updatePlugin");
        ASSERT_EQ(update.exitCode, 0) << update.output.toStdString();
    }
    {
        langscore::CSVEditorTableModel model;
        model.setSettings(std::make_shared<settings>(setting));
        ASSERT_TRUE(model.loadFromJsonFile(setting.analyzeDirectoryPath() + "/Actors.lsjson"));
        model.insertColumn(1, setting.defaultLanguage);
        model.insertColumn(2, targetLanguage);
        for(int row = 0; row < model.rowCount(); ++row)
        {
            const auto original = model.getCell(row, 0).value_or("");
            if(original.isEmpty()) { continue; }
            model.setData(model.index(row, 1), original);
            model.setData(model.index(row, 2), TranslatedPrefix + original);
        }
        const auto editingDir = setting.langscoreProjectDirectory + "/editing";
        QDir().mkpath(editingDir);
        ASSERT_TRUE(model.saveToFile(editingDir + "/Actors.csv"));
    }
    {
        const auto exportResult = RunDivisi(configPath, "--exportCSV");
        ASSERT_EQ(exportResult.exitCode, 0) << exportResult.output.toStdString();
        EXPECT_FALSE(langscore::ApplyEditingTranslations(setting).isEmpty());
        if(type == settings::VXAce)
        {
            const auto packing = RunDivisi(configPath, "--packing");
            ASSERT_EQ(packing.exitCode, 0) << packing.output.toStdString();
            langscore::RemovePackedCsvFiles(setting);
        }
    }

    //---- 3. テストプレイ ----
    const auto injectError = type == settings::VXAce ? InjectBootCheckVXAce(gameDir, tempDir.filePath("work")) : InjectBootCheckMvMz(gameDir);
    ASSERT_TRUE(injectError.isEmpty()) << injectError.toStdString();

    const auto [program, arguments] = RPGMakerPathDialog::testPlayCommand(type, gameDir, rpgMakerPath);
    QProcess game;
    game.setWorkingDirectory(gameDir);
    game.start(program, arguments);
    ASSERT_TRUE(game.waitForStarted()) << program.toStdString() << ": " << game.errorString().toStdString();
    if(game.waitForFinished(GameTimeoutMs) == false) {
        game.kill();
        game.waitForFinished();
    }

    const auto result = ReadText(gameDir + "/" + BootResultName);
    ASSERT_FALSE(result.isEmpty()) << "ゲームが結果を書かずに終了したか、時間内に終わりませんでした（起動中のエラーで止まった可能性があります）";
    const auto lines = result.split('\n', Qt::SkipEmptyParts);
    ASSERT_EQ(lines.last(), "DONE") << result.toStdString();
    EXPECT_TRUE(lines.contains("title")) << result.toStdString();

    //言語を切り替えると、アクター名が書き出した翻訳文になる
    QMap<QString, QString> actorNames;
    for(const auto& line : lines)
    {
        const auto columns = line.split('\t');
        if(columns.size() == 3 && columns[0] == "actor1") { actorNames[columns[1]] = columns[2]; }
    }
    ASSERT_TRUE(actorNames.contains(setting.defaultLanguage)) << result.toStdString();
    ASSERT_TRUE(actorNames.contains(targetLanguage)) << "ゲームで " << targetLanguage.toStdString() << " を選べません: " << result.toStdString();
    const auto original = actorNames[setting.defaultLanguage];
    EXPECT_FALSE(original.isEmpty());
    EXPECT_FALSE(original.startsWith(TranslatedPrefix)) << result.toStdString();
    EXPECT_EQ(actorNames[targetLanguage], TranslatedPrefix + original) << result.toStdString();
}

INSTANTIATE_TEST_SUITE_P(
    Platforms,
    FirstRunGamePlayTest,
    ::testing::Values(settings::VXAce, settings::MV, settings::MZ),
    [](const ::testing::TestParamInfo<settings::ProjectType>& info) {
        switch(info.param)
        {
        case settings::VXAce: return std::string("VXAce");
        case settings::MV:    return std::string("MV");
        case settings::MZ:    return std::string("MZ");
        default:              return std::string("Unknown");
        }
    }
);
