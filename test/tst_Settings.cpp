#include <QTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "settings.h"

class TestSettings : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void testProjectTypeFromFile_data();
    void testProjectTypeFromFile();
    void testProjectTypeFromDir();
    void testProjectTypeNone();
    void testSetGameProjectPathMV();
    void testSetGameProjectPathVXAce();
    void testSetGameProjectPathInvalid();
    void testFetchAndRemoveLanguage();
    void testLowerBcp47Name();
    void testIsLangscoreScript();
    void testValidateTextInfo();
    void testWriteLoadRoundTrip();

    // 不具合の再発防止
    void testSetGameProjectPathJapanese();
    void testLoadWithLangscoreInParentPath();
    void testScriptPartialIgnoreRoundTrip();
    void testScriptFullIgnoreKeepsLines();
    void testWriteFlagsRoundTrip();
    void testBasicDataVisibleRoundTrip();
    void testPictureVisibleRoundTrip();

private:
    std::unique_ptr<QTemporaryDir> tempDir;
    QString makeProject(const QString& name, const QString& projectFile);
    QString writeConfig(settings& s);
    void writeFile(const QString& path, const QByteArray& content);
};

QString TestSettings::writeConfig(settings& s)
{
    auto configPath = QDir::cleanPath(s.langscoreProjectDirectory) + "/config.json";
    s.write(configPath);
    return configPath;
}

void TestSettings::writeFile(const QString& path, const QByteArray& content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if(file.open(QFile::WriteOnly) == false){ qFatal("cannot create test file"); }
    file.write(content);
}

// analyze/Scripts.lsjson に Plugin.js の2行を用意した MZ プロジェクトを読み込む
static const QByteArray scriptsJson = R"([
    {"original": "A", "file": "Plugin.js", "row": 1, "col": 0},
    {"original": "B", "file": "Plugin.js", "row": 2, "col": 0}
])";

static settings::ScriptInfo* findScript(settings& s, const QString& fileName)
{
    auto& list = s.writeObj.scriptInfo;
    auto it = std::ranges::find(list, fileName, &settings::ScriptInfo::fileName);
    return it != list.end() ? &*it : nullptr;
}

static langscore::ScriptTextPosition* findLine(settings::ScriptInfo& info, size_t row)
{
    for(auto& line : info.lines) {
        if(std::get<langscore::ScriptTextPosition::RowCol>(line.d).row == row) { return &line; }
    }
    return nullptr;
}

void TestSettings::init()
{
    tempDir = std::make_unique<QTemporaryDir>();
}

QString TestSettings::makeProject(const QString& name, const QString& projectFile)
{
    QDir(tempDir->path()).mkdir(name);
    auto dir = tempDir->filePath(name);
    QFile file(dir + "/" + projectFile);
    if(file.open(QFile::WriteOnly) == false){ qFatal("cannot create test file"); }
    return dir;
}

void TestSettings::testProjectTypeFromFile_data()
{
    QTest::addColumn<QString>("fileName");
    QTest::addColumn<int>("expected");

    QTest::newRow("VXAce") << "Game.rvproj2" << int(settings::VXAce);
    QTest::newRow("MV")    << "Game.rpgproject" << int(settings::MV);
    QTest::newRow("MZ")    << "Game.rmmzproject" << int(settings::MZ);
    QTest::newRow("other") << "Game.txt" << int(settings::None);
}

void TestSettings::testProjectTypeFromFile()
{
    QFETCH(QString, fileName);
    QFETCH(int, expected);
    auto dir = makeProject("Game", fileName);
    QCOMPARE(int(settings::getProjectType(dir + "/" + fileName)), expected);
}

void TestSettings::testProjectTypeFromDir()
{
    auto dir = makeProject("Game", "Game.rmmzproject");
    QCOMPARE(settings::getProjectType(dir), settings::MZ);
}

void TestSettings::testProjectTypeNone()
{
    QCOMPARE(settings::getProjectType(tempDir->filePath("not_exists")), settings::None);
    QDir(tempDir->path()).mkdir("Empty");
    QCOMPARE(settings::getProjectType(tempDir->filePath("Empty")), settings::None);
}

void TestSettings::testSetGameProjectPathMV()
{
    auto dir = makeProject("Game", "Game.rpgproject");
    settings s;
    s.setGameProjectPath(dir);

    QCOMPARE(s.projectType, settings::MV);
    QCOMPARE(s.gameProjectPath, dir);
    QCOMPARE(s.writeObj.exportDirectory, QString("./data/translate"));
    QCOMPARE(QDir::cleanPath(s.langscoreProjectDirectory), QDir::cleanPath(tempDir->filePath("Game_langscore")));
    QCOMPARE(s.translateDirectoryPath(), dir + "/data/translate");
    QCOMPARE(s.tempGraphicsFileDirectoryPath(), dir + "/img");
    QCOMPARE(s.getDefaultFontSize(), 28u);
}

void TestSettings::testSetGameProjectPathVXAce()
{
    auto dir = makeProject("Game", "Game.rvproj2");
    settings s;
    s.setGameProjectPath(QDir::toNativeSeparators(dir));

    // バックスラッシュはスラッシュに正規化される
    QCOMPARE(s.gameProjectPath, dir);
    QCOMPARE(s.projectType, settings::VXAce);
    QCOMPARE(s.writeObj.exportDirectory, QString("./Data/Translate"));
    QCOMPARE(s.translateDirectoryPath(), dir + "/Data/Translate");
    QCOMPARE(s.tempGraphicsFileDirectoryPath(), dir + "/Graphics");
    QCOMPARE(settings::scriptExt(s.projectType), QString(".rb"));
}

void TestSettings::testSetGameProjectPathInvalid()
{
    QDir(tempDir->path()).mkdir("NotProject");
    settings s;
    s.setGameProjectPath(tempDir->filePath("NotProject"));
    QCOMPARE(s.projectType, settings::None);
    QVERIFY(s.gameProjectPath.isEmpty());
}

void TestSettings::testFetchAndRemoveLanguage()
{
    settings s;
    auto& en = s.fetchLanguage("en");
    en.enable = true;
    QCOMPARE(s.languages.size(), size_t{1});

    // 既存の言語は追加されず同じ要素を返す
    QVERIFY(s.fetchLanguage("en").enable);
    QCOMPARE(s.languages.size(), size_t{1});

    s.fetchLanguage("ja");
    QCOMPARE(s.languages.size(), size_t{2});

    s.removeLanguage("en");
    s.removeLanguage("xx");
    QCOMPARE(s.languages.size(), size_t{1});
    QCOMPARE(s.languages[0].languageName, QString("ja"));
}

void TestSettings::testLowerBcp47Name()
{
    QCOMPARE(settings::getLowerBcp47Name(QLocale(QLocale::English, QLocale::UnitedKingdom)), QString("en-gb"));
    // "zh" は "zh-cn" として扱う
    QCOMPARE(settings::getLowerBcp47Name(QLocale(QLocale::Chinese, QLocale::China)), QString("zh-cn"));
}

void TestSettings::testIsLangscoreScript()
{
    QVERIFY(settings::isLangscoreScript("Langscore"));
    QVERIFY(settings::isLangscoreScript("langscore_custom"));
    QVERIFY(settings::isLangscoreScript("Langscore_ObserverBridge"));
    QVERIFY(settings::isLangscoreScript("LangscoreX") == false);
    QVERIFY(settings::isLangscoreScript("") == false);
}

void TestSettings::testValidateTextInfo()
{
    settings::ValidateTextInfo a{settings::TextCount, 100, 10};
    settings::ValidateTextInfo b{settings::TextCount, 999, 10};
    // TextCount モードでは count のみ比較する
    QVERIFY(a == b);
    QCOMPARE(a.value(), 10);

    b.mode = settings::TextWidth;
    QVERIFY(a != b);
    QCOMPARE(b.value(), 999);
    b.setValue(50);
    QCOMPARE(b.width, 50);

    settings::ValidateTextInfo ignore{settings::Ignore, 1, 2};
    QVERIFY(ignore == settings::ValidateTextInfo{});
    QCOMPARE(ignore.value(), 0);
}

void TestSettings::testWriteLoadRoundTrip()
{
    auto dir = makeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.defaultLanguage = "en";
    s.fetchLanguage("en").enable = true;
    s.fetchLanguage("ja").enable = false;
    s.writeObj.exportByLanguage = true;
    s.writeObj.enableLanguagePatch = true;
    s.writeObj.enableAddNewTextToEnd = false;
    s.packingInputDirectory = "packing";

    auto configPath = QDir::cleanPath(s.langscoreProjectDirectory) + "/config.json";
    s.write(configPath);
    QVERIFY(QFile::exists(configPath));

    settings loaded;
    loaded.load(configPath);

    QCOMPARE(loaded.projectType, settings::MZ);
    QCOMPARE(QDir::cleanPath(loaded.gameProjectPath), dir);
    QCOMPARE(loaded.defaultLanguage, QString("en"));
    QCOMPARE(loaded.languages.size(), size_t{2});
    QCOMPARE(loaded.languages[0].languageName, QString("en"));
    QVERIFY(loaded.languages[0].enable);
    QCOMPARE(loaded.languages[1].languageName, QString("ja"));
    QVERIFY(loaded.languages[1].enable == false);
    QVERIFY(loaded.writeObj.exportByLanguage);
    QVERIFY(loaded.writeObj.enableLanguagePatch);
    QVERIFY(loaded.writeObj.enableAddNewTextToEnd == false);
    QCOMPARE(loaded.packingInputDirectory, QString("packing"));
}

void TestSettings::testSetGameProjectPathJapanese()
{
    // 日本語を含むフォルダ名でも例外を出さず、正しい langscore フォルダを求められること
    const QString name(u"\u30B2\u30FC\u30E0");
    auto dir = makeProject(name, "Game.rmmzproject");
    settings s;
    QVERIFY_THROWS_NO_EXCEPTION(s.setGameProjectPath(dir));
    QCOMPARE(s.projectType, settings::MZ);
    QCOMPARE(QDir::cleanPath(s.langscoreProjectDirectory), QDir::cleanPath(tempDir->filePath(name + "_langscore")));
}

void TestSettings::testLoadWithLangscoreInParentPath()
{
    // 親フォルダ名に "_langscore" を含んでも、末尾の "_langscore" だけを除去してゲームフォルダを求めること
    QDir(tempDir->path()).mkdir("my_langscore_work");
    auto dir = tempDir->filePath("my_langscore_work/Game");
    QDir().mkpath(dir);
    writeFile(dir + "/Game.rmmzproject", {});

    settings s;
    s.setGameProjectPath(dir);
    s.defaultLanguage = "en";
    auto configPath = writeConfig(s);

    settings loaded;
    loaded.load(configPath);
    QCOMPARE(loaded.projectType, settings::MZ);
    QCOMPARE(QDir::cleanPath(loaded.gameProjectPath), dir);
    QCOMPARE(loaded.defaultLanguage, QString("en"));
}

void TestSettings::testScriptPartialIgnoreRoundTrip()
{
    // 一部の行だけ無視したスクリプトの無視設定が、保存→読み込みで失われないこと
    auto dir = makeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    writeFile(s.analyzeDirectoryPath() + "/Scripts.lsjson", scriptsJson);
    auto configPath = writeConfig(s);

    settings first;
    first.load(configPath);
    auto* script = findScript(first, "Plugin.js");
    QVERIFY(script != nullptr);
    QCOMPARE(script->lines.size(), size_t{2});
    findLine(*script, 1)->ignore = true;
    writeConfig(first);

    settings second;
    second.load(configPath);
    script = findScript(second, "Plugin.js");
    QVERIFY(script != nullptr);
    QCOMPARE(script->lines.size(), size_t{2});
    QVERIFY(findLine(*script, 1) != nullptr);
    QVERIFY(findLine(*script, 1)->ignore);
    QVERIFY(findLine(*script, 2)->ignore == false);
}

void TestSettings::testScriptFullIgnoreKeepsLines()
{
    // 全行を無視したスクリプトも、読み込み後に原文と無視フラグが保持されていること
    auto dir = makeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    writeFile(s.analyzeDirectoryPath() + "/Scripts.lsjson", scriptsJson);
    auto configPath = writeConfig(s);

    settings first;
    first.load(configPath);
    auto* script = findScript(first, "Plugin.js");
    QVERIFY(script != nullptr);
    for(auto& line : script->lines) { line.ignore = true; }
    writeConfig(first);

    settings second;
    second.load(configPath);
    script = findScript(second, "Plugin.js");
    QVERIFY(script != nullptr);
    QCOMPARE(script->lines.size(), size_t{2});
    QVERIFY(findLine(*script, 1) != nullptr);
    QCOMPARE(findLine(*script, 1)->originalText, QString("A"));
    QVERIFY(findLine(*script, 1)->ignore);
    QVERIFY(findLine(*script, 2)->ignore);
}

void TestSettings::testWriteFlagsRoundTrip()
{
    // 書き出している OverwriteLangscore / OverwriteLangscoreCustom / WriteType が読み込みで復元されること
    auto dir = makeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.writeObj.overwriteLangscore = true;
    s.writeObj.overwriteLangscoreCustom = true;
    s.writeObj.writeMode = 2;
    auto configPath = writeConfig(s);

    settings loaded;
    loaded.load(configPath);
    QVERIFY(loaded.writeObj.overwriteLangscore);
    QVERIFY(loaded.writeObj.overwriteLangscoreCustom);
    QCOMPARE(loaded.writeObj.writeMode, 2);
}

void TestSettings::testBasicDataVisibleRoundTrip()
{
    // ツリーで非表示にした基本データ（IsVisible）が読み込みで復元されること
    auto dir = makeProject("Game", "Game.rmmzproject");
    settings s;
    s.setGameProjectPath(dir);
    s.fetchBasicDataInfo("Actors").visible = false;
    auto configPath = writeConfig(s);

    settings loaded;
    loaded.load(configPath);
    QVERIFY(loaded.fetchBasicDataInfo("Actors").visible == false);
}

void TestSettings::testPictureVisibleRoundTrip()
{
    // ツリーで非表示にした画像が読み込みで復元されること
    auto dir = makeProject("Game", "Game.rmmzproject");
    writeFile(dir + "/img/pictures/a.png", "dummy");
    settings s;
    s.setGameProjectPath(dir);
    auto configPath = writeConfig(s);

    auto findPicture = [](settings& target) -> settings::GraphInfo* {
        auto& list = target.writeObj.graphDataInfo;
        auto it = std::ranges::find_if(list, [](const auto& g) { return g.isFolder == false && g.fileName == "a"; });
        return it != list.end() ? &*it : nullptr;
    };

    settings first;
    first.load(configPath);
    auto* picture = findPicture(first);
    QVERIFY(picture != nullptr);
    picture->visible = false;
    writeConfig(first);

    settings second;
    second.load(configPath);
    picture = findPicture(second);
    QVERIFY(picture != nullptr);
    QVERIFY(picture->visible == false);
}

QTEST_MAIN(TestSettings)
#include "tst_Settings.moc"
