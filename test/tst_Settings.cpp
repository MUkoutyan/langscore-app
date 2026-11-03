#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "settings.h"

// analyze/Scripts.lsjson に Plugin.js の2行を用意した MZ プロジェクトを読み込む
static const QByteArray ScriptsJson = R"([
    {"original": "A", "file": "Plugin.js", "row": 1, "col": 0},
    {"original": "B", "file": "Plugin.js", "row": 2, "col": 0}
])";

static settings::ScriptInfo* FindScript(settings& s, const QString& fileName)
{
    auto& list = s.writeObj.scriptInfo;
    auto it = std::ranges::find(list, fileName, &settings::ScriptInfo::fileName);
    return it != list.end() ? &*it : nullptr;
}

static langscore::ScriptTextPosition* FindLine(settings::ScriptInfo& info, size_t row)
{
    for(auto& line : info.lines) {
        if(std::get<langscore::ScriptTextPosition::RowCol>(line.d).row == row) { return &line; }
    }
    return nullptr;
}

class SettingsTest : public ::testing::Test
{
protected:
    QTemporaryDir tempDir;

    QString MakeProject(const QString& name, const QString& projectFile)
    {
        QDir(tempDir.path()).mkdir(name);
        auto dir = tempDir.filePath(name);
        WriteFile(dir + "/" + projectFile, {});
        return dir;
    }

    QString WriteConfig(settings& s)
    {
        auto configPath = QDir::cleanPath(s.langscoreProjectDirectory) + "/config.json";
        s.write(configPath);
        return configPath;
    }

    void WriteFile(const QString& path, const QByteArray& content)
    {
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        if(file.open(QFile::WriteOnly) == false)
        {
            ADD_FAILURE() << "cannot create test file: " << path.toStdString();
            return;
        }
        file.write(content);
    }
};

TEST_F(SettingsTest, ProjectTypeFromFile)
{
    struct Row { const char* label; QString fileName; settings::ProjectType expected; };
    const Row rows[] = {
        {"VXAce", "Game.rvproj2",     settings::VXAce},
        {"MV",    "Game.rpgproject",  settings::MV},
        {"MZ",    "Game.rmmzproject", settings::MZ},
        {"other", "Game.txt",         settings::None},
    };
    for(const auto& row : rows)
    {
        SCOPED_TRACE(row.label);
        auto dir = MakeProject(row.label, row.fileName);
        EXPECT_EQ(settings::getProjectType(dir + "/" + row.fileName), row.expected);
    }
}

TEST_F(SettingsTest, ProjectTypeFromDir)
{
    auto dir = MakeProject("Game", "Game.rmmzproject");
    EXPECT_EQ(settings::getProjectType(dir), settings::MZ);
}

TEST_F(SettingsTest, ProjectTypeNone)
{
    EXPECT_EQ(settings::getProjectType(tempDir.filePath("not_exists")), settings::None);
    QDir(tempDir.path()).mkdir("Empty");
    EXPECT_EQ(settings::getProjectType(tempDir.filePath("Empty")), settings::None);
}

TEST_F(SettingsTest, SetGameProjectPathMV)
{
    auto dir = MakeProject("Game", "Game.rpgproject");
    settings s;
    s.setGameProjectPath(dir);

    EXPECT_EQ(s.projectType, settings::MV);
    EXPECT_EQ(s.gameProjectPath, dir);
    EXPECT_EQ(s.writeObj.exportDirectory, QString("./data/translate"));
    EXPECT_EQ(QDir::cleanPath(s.langscoreProjectDirectory), QDir::cleanPath(tempDir.filePath("Game_langscore")));
    EXPECT_EQ(s.translateDirectoryPath(), dir + "/data/translate");
    EXPECT_EQ(s.tempGraphicsFileDirectoryPath(), dir + "/img");
    EXPECT_EQ(s.getDefaultFontSize(), 28u);
}

TEST_F(SettingsTest, SetGameProjectPathVXAce)
{
    auto dir = MakeProject("Game", "Game.rvproj2");
    settings s;
    s.setGameProjectPath(QDir::toNativeSeparators(dir));

    // バックスラッシュはスラッシュに正規化される
    EXPECT_EQ(s.gameProjectPath, dir);
    EXPECT_EQ(s.projectType, settings::VXAce);
    EXPECT_EQ(s.writeObj.exportDirectory, QString("./Data/Translate"));
    EXPECT_EQ(s.translateDirectoryPath(), dir + "/Data/Translate");
    EXPECT_EQ(s.tempGraphicsFileDirectoryPath(), dir + "/Graphics");
    EXPECT_EQ(settings::scriptExt(s.projectType), QString(".rb"));
}

TEST_F(SettingsTest, SetGameProjectPathInvalid)
{
    QDir(tempDir.path()).mkdir("NotProject");
    settings s;
    s.setGameProjectPath(tempDir.filePath("NotProject"));
    EXPECT_EQ(s.projectType, settings::None);
    EXPECT_TRUE(s.gameProjectPath.isEmpty());
}

TEST_F(SettingsTest, FetchAndRemoveLanguage)
{
    settings s;
    auto& en = s.fetchLanguage("en");
    en.enable = true;
    EXPECT_EQ(s.languages.size(), size_t{1});

    // 既存の言語は追加されず同じ要素を返す
    EXPECT_TRUE(s.fetchLanguage("en").enable);
    EXPECT_EQ(s.languages.size(), size_t{1});

    s.fetchLanguage("ja");
    EXPECT_EQ(s.languages.size(), size_t{2});

    s.removeLanguage("en");
    s.removeLanguage("xx");
    ASSERT_EQ(s.languages.size(), size_t{1});
    EXPECT_EQ(s.languages[0].languageName, QString("ja"));
}

TEST_F(SettingsTest, LowerBcp47Name)
{
    EXPECT_EQ(settings::getLowerBcp47Name(QLocale(QLocale::English, QLocale::UnitedKingdom)), QString("en-gb"));
    // "zh" は "zh-cn" として扱う
    EXPECT_EQ(settings::getLowerBcp47Name(QLocale(QLocale::Chinese, QLocale::China)), QString("zh-cn"));
}

TEST_F(SettingsTest, IsLangscoreScript)
{
    EXPECT_TRUE(settings::isLangscoreScript("Langscore"));
    EXPECT_TRUE(settings::isLangscoreScript("langscore_custom"));
    EXPECT_TRUE(settings::isLangscoreScript("Langscore_ObserverBridge"));
    EXPECT_FALSE(settings::isLangscoreScript("LangscoreX"));
    EXPECT_FALSE(settings::isLangscoreScript(""));
}

TEST_F(SettingsTest, ValidateTextInfo)
{
    settings::ValidateTextInfo a{settings::TextCount, 100, 10};
    settings::ValidateTextInfo b{settings::TextCount, 999, 10};
    // TextCount モードでは count のみ比較する
    EXPECT_TRUE(a == b);
    EXPECT_EQ(a.value(), 10);

    b.mode = settings::TextWidth;
    EXPECT_TRUE(a != b);
    EXPECT_EQ(b.value(), 999);
    b.setValue(50);
    EXPECT_EQ(b.width, 50);

    settings::ValidateTextInfo ignore{settings::Ignore, 1, 2};
    EXPECT_TRUE(ignore == settings::ValidateTextInfo{});
    EXPECT_EQ(ignore.value(), 0);
}

TEST_F(SettingsTest, WriteLoadRoundTrip)
{
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.defaultLanguage = "en";
    s.fetchLanguage("en").enable = true;
    s.fetchLanguage("ja").enable = false;
    s.writeObj.exportByLanguage = true;
    s.writeObj.enableLanguagePatch = true;
    s.writeObj.enableAddNewTextToEnd = false;
    s.packingInputDirectory = "packing";

    auto configPath = WriteConfig(s);
    ASSERT_TRUE(QFile::exists(configPath));

    settings loaded;
    loaded.load(configPath);

    EXPECT_EQ(loaded.projectType, settings::MZ);
    EXPECT_EQ(QDir::cleanPath(loaded.gameProjectPath), dir);
    EXPECT_EQ(loaded.defaultLanguage, QString("en"));
    ASSERT_EQ(loaded.languages.size(), size_t{2});
    EXPECT_EQ(loaded.languages[0].languageName, QString("en"));
    EXPECT_TRUE(loaded.languages[0].enable);
    EXPECT_EQ(loaded.languages[1].languageName, QString("ja"));
    EXPECT_FALSE(loaded.languages[1].enable);
    EXPECT_TRUE(loaded.writeObj.exportByLanguage);
    EXPECT_TRUE(loaded.writeObj.enableLanguagePatch);
    EXPECT_FALSE(loaded.writeObj.enableAddNewTextToEnd);
    EXPECT_EQ(loaded.packingInputDirectory, QString("packing"));
}

// 不具合の再発防止
TEST_F(SettingsTest, SetGameProjectPathJapanese)
{
    // 日本語を含むフォルダ名でも例外を出さず、正しい langscore フォルダを求められること
    const QString name(u"ゲーム");
    auto dir = MakeProject(name, "Game.rmmzproject");
    settings s;
    EXPECT_NO_THROW(s.setGameProjectPath(dir));
    EXPECT_EQ(s.projectType, settings::MZ);
    EXPECT_EQ(QDir::cleanPath(s.langscoreProjectDirectory), QDir::cleanPath(tempDir.filePath(name + "_langscore")));
}

TEST_F(SettingsTest, LoadWithLangscoreInParentPath)
{
    // 親フォルダ名に "_langscore" を含んでも、末尾の "_langscore" だけを除去してゲームフォルダを求めること
    auto dir = tempDir.filePath("my_langscore_work/Game");
    WriteFile(dir + "/Game.rmmzproject", {});

    settings s;
    s.setGameProjectPath(dir);
    s.defaultLanguage = "en";
    auto configPath = WriteConfig(s);

    settings loaded;
    loaded.load(configPath);
    EXPECT_EQ(loaded.projectType, settings::MZ);
    EXPECT_EQ(QDir::cleanPath(loaded.gameProjectPath), dir);
    EXPECT_EQ(loaded.defaultLanguage, QString("en"));
}

TEST_F(SettingsTest, ScriptPartialIgnoreRoundTrip)
{
    // 一部の行だけ無視したスクリプトの無視設定が、保存→読み込みで失われないこと
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    WriteFile(s.analyzeDirectoryPath() + "/Scripts.lsjson", ScriptsJson);
    auto configPath = WriteConfig(s);

    settings first;
    first.load(configPath);
    auto* script = FindScript(first, "Plugin.js");
    ASSERT_NE(script, nullptr);
    ASSERT_EQ(script->lines.size(), size_t{2});
    FindLine(*script, 1)->ignore = true;
    WriteConfig(first);

    settings second;
    second.load(configPath);
    script = FindScript(second, "Plugin.js");
    ASSERT_NE(script, nullptr);
    ASSERT_EQ(script->lines.size(), size_t{2});
    ASSERT_NE(FindLine(*script, 1), nullptr);
    EXPECT_TRUE(FindLine(*script, 1)->ignore);
    EXPECT_FALSE(FindLine(*script, 2)->ignore);
}

TEST_F(SettingsTest, ScriptFullIgnoreKeepsLines)
{
    // 全行を無視したスクリプトも、読み込み後に原文と無視フラグが保持されていること
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    WriteFile(s.analyzeDirectoryPath() + "/Scripts.lsjson", ScriptsJson);
    auto configPath = WriteConfig(s);

    settings first;
    first.load(configPath);
    auto* script = FindScript(first, "Plugin.js");
    ASSERT_NE(script, nullptr);
    for(auto& line : script->lines) { line.ignore = true; }
    WriteConfig(first);

    settings second;
    second.load(configPath);
    script = FindScript(second, "Plugin.js");
    ASSERT_NE(script, nullptr);
    ASSERT_EQ(script->lines.size(), size_t{2});
    ASSERT_NE(FindLine(*script, 1), nullptr);
    EXPECT_EQ(FindLine(*script, 1)->originalText, QString("A"));
    EXPECT_TRUE(FindLine(*script, 1)->ignore);
    EXPECT_TRUE(FindLine(*script, 2)->ignore);
}

TEST_F(SettingsTest, WriteFlagsRoundTrip)
{
    // 書き出している OverwriteLangscore / OverwriteLangscoreCustom / WriteType が読み込みで復元されること
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.writeObj.overwriteLangscore = true;
    s.writeObj.overwriteLangscoreCustom = true;
    s.writeObj.writeMode = 2;
    auto configPath = WriteConfig(s);

    settings loaded;
    loaded.load(configPath);
    EXPECT_TRUE(loaded.writeObj.overwriteLangscore);
    EXPECT_TRUE(loaded.writeObj.overwriteLangscoreCustom);
    EXPECT_EQ(loaded.writeObj.writeMode, 2);
}

TEST_F(SettingsTest, BasicDataVisibleRoundTrip)
{
    // ツリーで非表示にした基本データ（IsVisible）が読み込みで復元されること
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.fetchBasicDataInfo("Actors").visible = false;
    auto configPath = WriteConfig(s);

    settings loaded;
    loaded.load(configPath);
    EXPECT_FALSE(loaded.fetchBasicDataInfo("Actors").visible);
}

TEST_F(SettingsTest, PictureVisibleRoundTrip)
{
    // ツリーで非表示にした画像が読み込みで復元されること
    auto dir = MakeProject("Game", "Game.rmmzproject");
    WriteFile(dir + "/img/pictures/a.png", "dummy");
    settings s;
    s.setGameProjectPath(dir);
    auto configPath = WriteConfig(s);

    auto findPicture = [](settings& target) -> settings::GraphInfo* {
        auto& list = target.writeObj.graphDataInfo;
        auto it = std::ranges::find_if(list, [](const auto& g) { return g.isFolder == false && g.fileName == "a"; });
        return it != list.end() ? &*it : nullptr;
    };

    settings first;
    first.load(configPath);
    auto* picture = findPicture(first);
    ASSERT_NE(picture, nullptr);
    picture->visible = false;
    WriteConfig(first);

    settings second;
    second.load(configPath);
    picture = findPicture(second);
    ASSERT_NE(picture, nullptr);
    EXPECT_FALSE(picture->visible);
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(SettingsTest, ScriptIgnoreAllLinesCanBeUnignored) // [5]
{
    // 全行を個別に無視して保存しても、読み込み後にスクリプト全体の無視にならないこと
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    WriteFile(s.analyzeDirectoryPath() + "/Scripts.lsjson", ScriptsJson);
    auto configPath = WriteConfig(s);

    settings first;
    first.load(configPath);
    auto* script = FindScript(first, "Plugin.js");
    ASSERT_NE(script, nullptr);
    for(auto& line : script->lines) { line.ignore = true; }
    WriteConfig(first);

    settings second;
    second.load(configPath);
    script = FindScript(second, "Plugin.js");
    ASSERT_NE(script, nullptr);
    EXPECT_FALSE(script->ignore);

    // 1行だけ無視を外せば、スクリプトは無視の扱いでなくなること
    ASSERT_NE(FindLine(*script, 1), nullptr);
    FindLine(*script, 1)->ignore = false;
    EXPECT_FALSE(script->isIgnore());
}

TEST_F(SettingsTest, ValidationDataRefForMap) // [6]
{
    // マップの検証設定は mapDataInfo の各要素を参照し、マップどうしで共有されないこと
    settings s;
    settings::MapInfo map1;
    map1.fileName = "Map001.json";
    settings::MapInfo map2;
    map2.fileName = "Map002.json";
    s.writeObj.mapDataInfo = {map1, map2};

    auto& ref1 = s.getValidationCsvDataRef("Map001");
    auto& ref2 = s.getValidationCsvDataRef("Map002");
    EXPECT_EQ(&ref1, &s.writeObj.mapDataInfo[0].validateInfo);
    EXPECT_EQ(&ref2, &s.writeObj.mapDataInfo[1].validateInfo);
}

TEST_F(SettingsTest, IgnorePictureSameNameInOtherFolder) // [8]
{
    // 除外した画像と同じ名前でも、別のフォルダにある画像は除外されないこと
    auto dir = MakeProject("Game", "Game.rmmzproject");
    WriteFile(dir + "/img/faces/Actor1.png", "dummy");
    WriteFile(dir + "/img/characters/Actor1.png", "dummy");
    settings s;
    s.setGameProjectPath(dir);
    //divisi と同じ形式（ゲームフォルダからの相対パス・拡張子なし）で保存する [19]
    s.writeObj.ignorePicturePath = {"img/faces/Actor1"};
    auto configPath = WriteConfig(s);

    settings loaded;
    loaded.load(configPath);
    auto findPicture = [&loaded](const QString& folder) -> settings::GraphInfo* {
        auto& list = loaded.writeObj.graphDataInfo;
        auto it = std::ranges::find_if(list, [&folder](const auto& g) {
            return g.isFolder == false && g.fileName == "Actor1" && QDir::fromNativeSeparators(g.folder) == folder;
        });
        return it != list.end() ? &*it : nullptr;
    };
    auto* face = findPicture("faces");
    auto* character = findPicture("characters");
    ASSERT_NE(face, nullptr);
    ASSERT_NE(character, nullptr);
    EXPECT_TRUE(face->ignore);
    EXPECT_FALSE(character->ignore);
}

TEST_F(SettingsTest, LoadTwiceKeepsControlCharList) // [13]
{
    // 同じ設定を読み込み直しても、制御文字リストが重複しないこと
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.validateObj.controlCharList = {"\\C"};
    auto configPath = WriteConfig(s);

    settings loaded;
    loaded.load(configPath);
    loaded.load(configPath);
    EXPECT_EQ(loaded.validateObj.controlCharList, QStringList{"\\C"});
}

TEST_F(SettingsTest, SetupFontListTwiceNoDuplicate) // [17]
{
    // フォント一覧を作り直しても、同じフォントが重複して登録されないこと
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    //中身が不正なフォントでも、Fonts フォルダにあるファイルは一覧に追加される
    WriteFile(s.langscoreProjectDirectory + "/Fonts/dummy.ttf", "dummy");

    s.setupFontList();
    const auto firstCount = s.fontIndexList.size();
    ASSERT_LT(0u, firstCount);
    s.setupFontList();
    EXPECT_EQ(s.fontIndexList.size(), firstCount);
}

TEST_F(SettingsTest, ScriptIgnorePointReadableByDivisi) // [18]
{
    // 無視した行は、divisi が無視対象として扱う形式（Ignore キーが無いか false）で保存されること
    //divisi は Ignore が true の項目を読み飛ばす（javascriptreader.hpp / rubyreader.hpp の applyIgnoreScriptsAndSwapLineInfo）
    auto dir = MakeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    WriteFile(s.analyzeDirectoryPath() + "/Scripts.lsjson", ScriptsJson);
    auto configPath = WriteConfig(s);

    settings loaded;
    loaded.load(configPath);
    auto* script = FindScript(loaded, "Plugin.js");
    ASSERT_NE(script, nullptr);
    ASSERT_NE(FindLine(*script, 1), nullptr);
    FindLine(*script, 1)->ignore = true;

    const auto root = QJsonDocument::fromJson(loaded.createJson()).object();
    const auto scripts = root["Write"].toObject()["RPGMakerScripts"].toArray();
    ASSERT_EQ(scripts.size(), 1);
    const auto points = scripts[0].toObject()["IgnorePoints"].toArray();
    ASSERT_EQ(points.size(), 1);
    const auto point = points[0].toObject();
    EXPECT_EQ(point["Row"].toInteger(), 1);
    EXPECT_FALSE(point["Ignore"].toBool(false));
}
