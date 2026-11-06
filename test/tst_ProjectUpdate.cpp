#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDialog>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QApplication>
#include "WriteModeComponent.h"
#include "MainCSVTable.h"
#include "EditDataManager.h"
#include "MainCSVTableModel.h"
#include <QAbstractProxyModel>
#include <QItemSelectionModel>
#include "CSVEditor.h"

// setting を参照するために公開する
struct ProjectUpdateTestComponent : public ComponentBase
{
    using ComponentBase::setting;
    using ComponentBase::runtimeData;
};

namespace
{
    void WriteTextFile(const QString& path, const QByteArray& content)
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

    //MZ のゲームプロジェクトを作り、設定を読み込む
    void SetupProject(ProjectUpdateTestComponent& component, const QString& gameDir)
    {
        WriteTextFile(gameDir + "/Game.rmmzproject", "RPGMZ");
        auto& setting = *component.setting;
        setting.setGameProjectPath(gameDir);
        WriteTextFile(setting.analyzeDirectoryPath() + "/Actors.json", "[]");
        const auto configPath = setting.langscoreProjectDirectory + "/config.json";
        setting.write(configPath);
        setting.load(configPath);
    }

    //テストの間だけ、何もせずに成功する divisi.exe を exe の隣の bin に置く
    class ScopedFakeDivisi
    {
    public:
        ScopedFakeDivisi()
            : path(QCoreApplication::applicationDirPath() + "/bin/divisi.exe")
        {
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile::remove(path);
            copied = QFile::copy(LANGSCORE_FAKE_DIVISI_PATH, path);
        }

        ~ScopedFakeDivisi()
        {
            QFile::remove(path);
        }

        ScopedFakeDivisi(const ScopedFakeDivisi&) = delete;
        ScopedFakeDivisi& operator=(const ScopedFakeDivisi&) = delete;

        bool valid() const { return copied; }

    private:
        QString path;
        bool copied = false;
    };
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST(ProjectUpdateTest, UpdateReloadsFileList) // [2]
{
    // 更新（再解析）が成功したら、解析フォルダに増えたファイルがファイル一覧に反映されること
    QTemporaryDir tempDir;
    ProjectUpdateTestComponent component;
    SetupProject(component, tempDir.filePath("Game"));
    const auto& setting = *component.setting;

    WriteModeComponent writeMode(&component, nullptr);
    auto* updateButton = writeMode.findChild<QPushButton*>("updateButton");
    ASSERT_NE(updateButton, nullptr);

    ScopedFakeDivisi fakeDivisi;
    ASSERT_TRUE(fakeDivisi.valid());

    //divisi の再解析で、ゲームに追加されたデータが解析フォルダに出力された状態にする
    WriteTextFile(setting.analyzeDirectoryPath() + "/Items.json", "[]");

    //更新の確認ダイアログを開いたら承認する
    QTimer::singleShot(0, []() {
        if(auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) {
            dialog->accept();
        }
    });
    updateButton->click();

    //処理中はボタンが無効になり、divisi の終了後に有効へ戻る
    ASSERT_TRUE(WaitUntil([updateButton]() { return updateButton->isEnabled(); }, 10000));
    //divisi の起動に失敗した場合（Failed.）ではなく、更新の成功時の処理まで進んだことを確かめる
    auto* logText = writeMode.findChild<QPlainTextEdit*>("logText");
    ASSERT_NE(logText, nullptr);
    ASSERT_TRUE(logText->toPlainText().contains("Done.")) << logText->toPlainText().toStdString();

    const auto& basicList = setting.getBasicInfoList();
    const bool hasItems = std::ranges::any_of(basicList, [](const auto& info) { return info.fileName == "Items.json"; });
    EXPECT_TRUE(hasItems) << "更新後のファイル一覧に、解析フォルダに増えた Items.json がありません";
}

//-------------------------------------------------------------------
// [3] 一度開いたファイルの表を、更新後の解析結果（lsjson）から作り直すこと
// 行は lsjson から作り、editing に保存したCSVからは翻訳だけを原文で比較して戻す。
//-------------------------------------------------------------------
namespace
{
    //言語を設定する。並び順が表の言語列の並び順になる
    void SetLanguages(settings& setting, const std::vector<std::pair<QString, bool>>& languages)
    {
        setting.languages.clear();
        for(const auto& [name, enable] : languages) {
            setting.languages.emplace_back(settings::Language{.languageName = name, .enable = enable});
        }
    }

    std::optional<size_t> FindColumn(const langscore::CSVEditorTableModel& model, const QString& header)
    {
        for(int column = 0; column < model.columnCount(); ++column)
        {
            if(model.headerData(column, Qt::Horizontal, Qt::UserRole).toString() == header) {
                return static_cast<size_t>(column);
            }
        }
        return std::nullopt;
    }

    std::optional<size_t> FindRow(const langscore::CSVEditorTableModel& model, const QString& original)
    {
        for(size_t row = 0; row < static_cast<size_t>(model.rowCount()); ++row)
        {
            if(model.getCell(row, 0).value_or("") == original) {
                return row;
            }
        }
        return std::nullopt;
    }

    //original 列が original の行の、header 列の値を返す。行か列が無ければ nullopt
    std::optional<QString> FindCell(const langscore::CSVEditorTableModel& model, const QString& original, const QString& header)
    {
        const auto row = FindRow(model, original);
        const auto column = FindColumn(model, header);
        if(row.has_value() == false || column.has_value() == false) { return std::nullopt; }
        return model.getCell(*row, *column);
    }

    //メインの表で Actors を開いたときのモデルを返す
    MainCSVTableModel* OpenActors(ProjectUpdateTestComponent& component, const std::shared_ptr<EditDataManager>& manager, MainCSVTable& table)
    {
        table.showMainFileText("Actors", "Actors");
        return manager->getMainCSVModel(component.setting->langscoreProjectDirectory + "/editing/Actors.csv");
    }

    class EditedCsvReloadTest : public ::testing::Test
    {
    protected:
        QTemporaryDir tempDir;
        ProjectUpdateTestComponent component;
        QString editingCsv;

        void SetUp() override
        {
            SetupProject(component, tempDir.filePath("Game"));
            SetLanguages(*component.setting, {{"en", true}});
            editingCsv = component.setting->langscoreProjectDirectory + "/editing/Actors.csv";
        }

        void WriteAnalyzeResult(const QByteArray& lsjson)
        {
            WriteTextFile(component.setting->analyzeDirectoryPath() + "/Actors.lsjson", lsjson);
        }
    };
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(EditedCsvReloadTest, IncludesNewTexts) // [3]
{
    // editing に保存したCSVがあっても、更新で増えた文章が表に出て、保存した翻訳は残ること
    WriteAnalyzeResult(R"([
        {"original": "A", "type": ["name"]},
        {"original": "B", "type": ["name"]}
    ])");
    //更新前に A だけを翻訳して保存した CSV
    WriteTextFile(editingCsv, "original,en,type\nA,Hello,name\n");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);

    EXPECT_TRUE(FindRow(*model, "B").has_value()) << "更新で増えた文章 B が表にありません";
    EXPECT_EQ(FindCell(*model, "A", "en"), QString("Hello")) << "保存した翻訳が失われています";
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(EditedCsvReloadTest, RemovedTextIsDropped) // [3]
{
    // ゲームから削除した文章は、editing のCSVに翻訳があっても表に残らないこと
    WriteAnalyzeResult(R"([
        {"original": "A", "type": ["name"]}
    ])");
    WriteTextFile(editingCsv, "original,en,type\nA,Hello,name\nC,Removed,name\n");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);

    EXPECT_FALSE(FindRow(*model, "C").has_value()) << "ゲームから削除した文章 C が表に残っています";
    EXPECT_EQ(FindCell(*model, "A", "en"), QString("Hello"));
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(EditedCsvReloadTest, TranslationMatchedByOriginal) // [3]
{
    // 解析結果と editing のCSVで行の並びが違っても、翻訳は原文が同じ行に戻ること
    WriteAnalyzeResult(R"([
        {"original": "B", "type": ["name"]},
        {"original": "A", "type": ["name"]}
    ])");
    WriteTextFile(editingCsv, "original,en,type\nA,Hello,name\nB,World,name\n");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);

    EXPECT_EQ(FindCell(*model, "A", "en"), QString("Hello"));
    EXPECT_EQ(FindCell(*model, "B", "en"), QString("World"));
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(EditedCsvReloadTest, SavedTranslationSurvivesReopen) // [3]
{
    // アプリで翻訳して保存したファイルを開き直しても、翻訳が残ること。
    // 原文にはCSVでエスケープが必要な文字（, " 改行）を含める
    const QString original = "a,\"b\"\nc";
    WriteAnalyzeResult(R"([
        {"original": "a,\"b\"\nc", "type": ["message"]}
    ])");

    {
        auto manager = std::make_shared<EditDataManager>();
        MainCSVTable table(&component, manager);
        auto* model = OpenActors(component, manager, table);
        ASSERT_NE(model, nullptr);
        const auto row = FindRow(*model, original);
        const auto column = FindColumn(*model, "en");
        ASSERT_TRUE(row.has_value());
        ASSERT_TRUE(column.has_value());
        model->setCell(*row, *column, "Translated");
        ASSERT_TRUE(manager->saveAllModels(component.setting->langscoreProjectDirectory + "/editing"));
    }

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(FindCell(*model, original, "en"), QString("Translated"));
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(EditedCsvReloadTest, DisabledLanguageIsKeptOnSave) // [3]
{
    // 無効にした言語の翻訳も、開いて保存したときに editing のCSVから消えないこと
    SetLanguages(*component.setting, {{"en", true}, {"fr", false}});
    WriteAnalyzeResult(R"([
        {"original": "A", "type": ["name"]}
    ])");
    WriteTextFile(editingCsv, "original,en,fr,type\nA,Hello,Bonjour,name\n");

    {
        auto manager = std::make_shared<EditDataManager>();
        MainCSVTable table(&component, manager);
        ASSERT_NE(OpenActors(component, manager, table), nullptr);
        ASSERT_TRUE(manager->saveAllModels(component.setting->langscoreProjectDirectory + "/editing"));
    }

    langscore::CSVEditorTableModel saved(editingCsv);
    EXPECT_EQ(FindCell(saved, "A", "fr"), QString("Bonjour")) << "無効にした言語の翻訳が保存で失われています";
    EXPECT_EQ(FindCell(saved, "A", "en"), QString("Hello"));
}

// 不具合の再発防止
TEST_F(EditedCsvReloadTest, LoadAllMainFilesWritesUnopenedFiles)
{
    // 全検証の前に、一度も開いていないファイルも editing に保存されること（divisi は editing のCSVだけを検証する）
    component.setting->fetchBasicDataInfo("Armors");
    WriteTextFile(component.setting->analyzeDirectoryPath() + "/Armors.lsjson", R"([
        {"original": "Sword", "type": ["name"]}
    ])");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    table.loadAllMainFiles();
    ASSERT_TRUE(manager->saveAllModels(component.setting->langscoreProjectDirectory + "/editing"));

    const auto armorsCsv = component.setting->langscoreProjectDirectory + "/editing/Armors.csv";
    ASSERT_TRUE(QFile::exists(armorsCsv)) << "開いていない Armors が editing に保存されていません";
    langscore::CSVEditorTableModel saved(armorsCsv);
    EXPECT_TRUE(FindRow(saved, "Sword").has_value());
    EXPECT_TRUE(FindColumn(saved, "en").has_value()) << "言語列がありません";
}

TEST_F(EditedCsvReloadTest, SelectErrorSelectsItsCell)
{
    // 検証結果を指定すると、その行・言語のセルが選択されること
    WriteAnalyzeResult(R"([
        {"original": "A", "type": ["name"]},
        {"original": "B", "type": ["name"]}
    ])");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);

    //CSV の 2 行目（ヘッダーの次の次）= 原文 B の行の en 列
    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Warning;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 2;
    info.language = "en";
    info.id = 7;
    component.runtimeData->errors["Actors"].push_back(info);

    table.selectError(7);
    QCoreApplication::processEvents();

    //検証結果の一覧（QTableWidget）と取り違えないよう、編集表の型で探す
    auto* view = table.findChild<CSVEditor*>();
    ASSERT_NE(view, nullptr);
    const auto selected = view->selectionModel()->selectedIndexes();
    ASSERT_EQ(selected.size(), 1);
    const auto* proxy = qobject_cast<const QAbstractProxyModel*>(view->model());
    const auto source = proxy ? proxy->mapToSource(selected[0]) : selected[0];
    EXPECT_EQ(model->getCell(source.row(), 0).value_or(""), QString("B"));
    EXPECT_EQ(model->headerData(source.column(), Qt::Horizontal, Qt::UserRole).toString(), QString("en"));
}

//-------------------------------------------------------------------
// 日本語・空白・CP932 に無い文字（ハングル）を含むフォルダでも、
// 表の読み込み・保存・開き直し・進捗の計算ができること
//-------------------------------------------------------------------
namespace
{
    class JapanesePathTest : public EditedCsvReloadTest
    {
    protected:
        void SetUp() override
        {
            SetupProject(component, tempDir.filePath(QString(u"日本語 フォルダ/ゲーム_한글")));
            SetLanguages(*component.setting, {{"en", true}});
            editingCsv = component.setting->langscoreProjectDirectory + "/editing/Actors.csv";
        }
    };
}

TEST_F(JapanesePathTest, ProjectPath)
{
    // 設定の保存・読み込みで、日本語のパスが化けないこと
    EXPECT_TRUE(component.setting->gameProjectPath.endsWith(QString(u"日本語 フォルダ/ゲーム_한글")));
    EXPECT_TRUE(QFile::exists(component.setting->langscoreProjectDirectory + "/config.json"));
    //列の表示設定の保存先は、日本語を含まないキーになること
    const auto group = langscore::CSVEditorSortFilterProxyModel::settingsGroupForProject(component.setting->langscoreProjectDirectory);
    EXPECT_TRUE(std::ranges::all_of(group, [](QChar c) { return c.unicode() < 0x80; })) << group.toStdString();
}

TEST_F(JapanesePathTest, SavedTranslationSurvivesReopen)
{
    // 日本語のフォルダに保存した翻訳を、開き直して読めること
    WriteAnalyzeResult(R"([{"original": "勇者", "type": ["name"]}])");
    {
        auto manager = std::make_shared<EditDataManager>();
        MainCSVTable table(&component, manager);
        auto* model = OpenActors(component, manager, table);
        ASSERT_NE(model, nullptr);
        model->setCell(*FindRow(*model, QString(u"勇者")), *FindColumn(*model, "en"), "Hero");
        ASSERT_TRUE(manager->saveAllModels(component.setting->langscoreProjectDirectory + "/editing"));
    }
    ASSERT_TRUE(QFile::exists(editingCsv));

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(FindCell(*model, QString(u"勇者"), "en"), QString("Hero"));
}

TEST_F(JapanesePathTest, ProgressOfUnopenedFile)
{
    // 開いていないファイルの進捗を、保存した翻訳から数えられること。数えるだけで編集データは作らないこと
    WriteAnalyzeResult(R"([
        {"original": "A", "type": ["name"]},
        {"original": "B", "type": ["name"]}
    ])");
    WriteTextFile(editingCsv, "original,en,type\nA,Hello,name\n");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    std::optional<std::pair<int, int>> progress;
    QObject::connect(&table, &MainCSVTable::fileProgressChanged, [&progress](const QString& fileName, int translatedRows, int totalRows) {
        if(fileName.startsWith("Actors")) {
            progress = std::make_pair(translatedRows, totalRows);
        }
    });
    table.notifyAllFileProgress();

    ASSERT_TRUE(progress.has_value());
    EXPECT_EQ(*progress, std::make_pair(1, 2));
    EXPECT_EQ(manager->findMainCSVModel(editingCsv), nullptr);
}

// 不具合の再発防止：開いた後に有効にした言語の列が表に出なかった
TEST_F(EditedCsvReloadTest, LanguageEnabledAfterOpenAddsColumn)
{
    // ファイルを開いた後に言語を有効にしても、開き直せばその言語の列があること
    WriteAnalyzeResult(R"([{"original": "A", "type": ["name"]}])");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    ASSERT_NE(OpenActors(component, manager, table), nullptr);

    //言語の切り替え（LanguageSelectComponent）は setting の enable を変えるだけ
    SetLanguages(*component.setting, {{"en", true}, {"fr", true}});
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    EXPECT_TRUE(FindColumn(*model, "fr").has_value()) << "後から有効にした fr の列がありません";
}
