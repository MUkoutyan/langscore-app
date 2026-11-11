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
#include <QMessageBox>
#include <QTableWidget>
#include <QComboBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <functional>
#include "WriteModeComponent.h"
#include "MainCSVTable.h"
#include "EditDataManager.h"
#include "MainCSVTableModel.h"
#include <QAbstractProxyModel>
#include <QItemSelectionModel>
#include "CSVEditor.h"
#include "dialog/AnalyzeDialog.h"
#include <QCheckBox>
#include <QTabWidget>
#include <QStandardPaths>
#include <QProcess>
#include <filesystem>
#include <memory>
#include "FileTree.h"
#ifdef Q_OS_WIN
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
    // 再解析による更新が成功したら、解析フォルダに増えたファイルがファイル一覧に反映されること
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
    //divisi の起動失敗による Failed. ではなく、更新の成功時の処理まで進んだことを確認する
    auto* logText = writeMode.findChild<QPlainTextEdit*>("logText");
    ASSERT_NE(logText, nullptr);
    ASSERT_TRUE(logText->toPlainText().contains("Done.")) << logText->toPlainText().toStdString();

    const auto& basicList = setting.getBasicInfoList();
    const bool hasItems = std::ranges::any_of(basicList, [](const auto& info) { return info.fileName == "Items.json"; });
    EXPECT_TRUE(hasItems) << "更新後のファイル一覧に、解析フォルダに増えた Items.json がありません";
}

//-------------------------------------------------------------------
// [3] 一度開いたファイルの表を、更新後の解析結果である lsjson から再構築すること
// 行は lsjson から作成し、editing に保存した CSV からは翻訳文のみを原文で照合して戻す。
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
    //更新前に A のみを翻訳して保存した CSV
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
    // 原文には , " 改行など、CSV でエスケープが必要な文字を含める
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
    // 無効にした言語の翻訳文も、開いて保存したときに editing の CSV から削除されないこと
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
    // divisi は editing の CSV のみ検証するため、全検証の前に、一度も開いていないファイルも editing に保存されること
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

    //ヘッダーの次の次である CSV の 2 行目 = 原文 B の行の en 列
    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Warning;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 2;
    info.language = "en";
    info.id = 7;
    component.runtimeData->errors["Actors"].push_back(info);

    table.selectError(7);
    QCoreApplication::processEvents();

    //検証結果の一覧である QTableWidget と取り違えないよう、編集表の型で探す
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
// 日本語・空白・ハングルなど CP932 に無い文字を含むフォルダでも、
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
    // 未表示のファイルの進捗を、保存した翻訳文から数えられること。数えるのみで編集データは作成しないこと
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

    //LanguageSelectComponent による言語の切り替えは、setting の enable を変更するのみ
    SetLanguages(*component.setting, {{"en", true}, {"fr", true}});
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    EXPECT_TRUE(FindColumn(*model, "fr").has_value()) << "後から有効にした fr の列がありません";
}

TEST(ProjectUpdateTest, StartTranslationExportsInAnalyzeDialog)
{
    // 翻訳作業の開始では、解析の画面のまま解析・プラグインの追加・翻訳CSVの書き出しまで行い、
    // 完了後に編集画面へ切り替えること。プラグインオプションも書き出しに使われること
    QTemporaryDir tempDir;
    ProjectUpdateTestComponent component;
    const auto gameDir = tempDir.filePath("Game");
    SetupProject(component, gameDir);
    auto& setting = *component.setting;
    setting.isPluginRegistered = false;
    setting.languages.clear();

    AnalyzeDialog dialog(&component);
    dialog.openFile(gameDir);
    auto* startButton = dialog.findChild<QPushButton*>("analyzeButton");
    auto* remoteHub = dialog.findChild<QCheckBox*>("enableRemoteHub");
    ASSERT_NE(startButton, nullptr);
    ASSERT_NE(remoteHub, nullptr);
    ASSERT_TRUE(startButton->isEnabled());
    remoteHub->setChecked(true);

    QStringList events;
    QObject::connect(&dialog, &AnalyzeDialog::translationStarted, [&events]() { events << "started"; });
    QObject::connect(&dialog, &AnalyzeDialog::toWriteMode, [&events, &setting](QString) {
        //編集画面へ切り替える時点で、ゲームへの書き出しは完了済
        events << (setting.isPluginRegistered ? "toWriteMode" : "toWriteModeBeforeExport");
    });

    ScopedFakeDivisi fakeDivisi;
    ASSERT_TRUE(fakeDivisi.valid());
    startButton->click();
    ASSERT_TRUE(WaitUntil([&events]() { return events.size() == 2; }, 10000));
    EXPECT_EQ(events, QStringList({"started", "toWriteMode"}));

    //書き出しの前に言語を設定し、書き出し先はゲームの翻訳フォルダにする。保存した設定にも残る
    settings saved;
    saved.load(setting.langscoreProjectDirectory + "/config.json");
    EXPECT_TRUE(saved.isPluginRegistered);
    EXPECT_TRUE(saved.isFirstExported);
    EXPECT_TRUE(saved.writeObj.enableRemoteHub);
    EXPECT_EQ(saved.languages.size(), settings::supportedLocales().size());
    EXPECT_EQ(QDir::cleanPath(QDir(setting.langscoreProjectDirectory).absoluteFilePath(setting.writeObj.exportDirectory)), QDir::cleanPath(setting.translateDirectoryPath()));
}

TEST(ProjectUpdateTest, LogTabReturnsToPreviousTabOnlyOnSuccess)
{
    // 処理が成功したらログを表示する前のタブに戻し、失敗したらログを表示したままにする
    QTemporaryDir tempDir;
    ProjectUpdateTestComponent component;
    SetupProject(component, tempDir.filePath("Game"));

    WriteModeComponent writeMode(&component, nullptr);
    writeMode.show();
    auto* tabs = writeMode.findChild<QTabWidget*>("tabWidget");
    auto* updateButton = writeMode.findChild<QPushButton*>("updateButton");
    ASSERT_NE(tabs, nullptr);
    ASSERT_NE(updateButton, nullptr);
    ASSERT_TRUE(writeMode.findChild<FileTree*>()->selectFile("Actors"));
    ASSERT_EQ(tabs->currentWidget()->objectName(), QString("mainTab"));

    const auto runUpdate = [&]() {
        QTimer::singleShot(0, []() {
            if(auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget())) { dialog->accept(); }
        });
        updateButton->click();
        return WaitUntil([updateButton]() { return updateButton->isEnabled(); }, 10000);
    };

    {
        ScopedFakeDivisi fakeDivisi;
        ASSERT_TRUE(fakeDivisi.valid());
        ASSERT_TRUE(runUpdate());
    }
    EXPECT_EQ(tabs->currentWidget()->objectName(), QString("mainTab"));

    //divisi.exe が無いと起動に失敗する
    ASSERT_TRUE(runUpdate());
    EXPECT_EQ(tabs->currentWidget()->objectName(), QString("logTab"));
}

TEST(ProjectUpdateTest, RestoresLastViewedFileAndTab)
{
    // 初めて開いたプロジェクトは言語のタブを表示する。閉じて開き直したら、最後に表示していたファイルとタブを復元する
    QTemporaryDir tempDir;
    ProjectUpdateTestComponent component;
    SetupProject(component, tempDir.filePath("Game"));

    QString viewedFile;
    {
        WriteModeComponent writeMode(&component, nullptr);
        writeMode.show();
        auto* tabs = writeMode.findChild<QTabWidget*>("tabWidget");
        auto* fileTree = writeMode.findChild<FileTree*>();
        ASSERT_NE(tabs, nullptr);
        ASSERT_NE(fileTree, nullptr);
        EXPECT_EQ(tabs->currentWidget()->objectName(), QString("langTab"));

        ASSERT_TRUE(fileTree->selectFile("Actors"));
        viewedFile = fileTree->currentFile();
        ASSERT_FALSE(viewedFile.isEmpty());
        EXPECT_EQ(tabs->currentWidget()->objectName(), QString("mainTab"));
    }

    WriteModeComponent reopened(&component, nullptr);
    reopened.show();
    EXPECT_EQ(reopened.findChild<QTabWidget*>("tabWidget")->currentWidget()->objectName(), QString("mainTab"));
    EXPECT_EQ(reopened.findChild<FileTree*>()->currentFile(), viewedFile);
}

namespace
{
    //衝突のダイアログが表示されたら handle で操作する。表示されなければ false
    bool HandleConflictDialog(const std::function<void(QDialog&, QTableWidget&)>& handle)
    {
        bool handled = false;
        QTimer poll;
        QObject::connect(&poll, &QTimer::timeout, [&handled, &handle]() {
            auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
            if(dialog == nullptr || dialog->objectName() != "mergeConflictDialog") { return; }
            auto* table = dialog->findChild<QTableWidget*>("conflictTable");
            if(table == nullptr) { return; }
            handled = true;
            handle(*dialog, *table);
        });
        poll.start(50);
        return WaitUntil([&handled]() { return handled; }, 5000);
    }

    //A・B の en に保存した後、開いたまま 0.5 秒以上待って監視が始まった状態にする
    MainCSVTableModel* OpenSavedActors(ProjectUpdateTestComponent& component, const std::shared_ptr<EditDataManager>& manager, MainCSVTable& table)
    {
        auto* model = OpenActors(component, manager, table);
        if(model == nullptr) { return nullptr; }
        const auto column = *FindColumn(*model, "en");
        model->setData(model->index(*FindRow(*model, "A"), column), "SavedA");
        model->setData(model->index(*FindRow(*model, "B"), column), "SavedB");
        manager->saveAllModels(component.setting->langscoreProjectDirectory + "/editing");
        WaitUntil([]() { return false; }, 1500);
        return model;
    }
}

// 不具合の再発防止
TEST_F(EditedCsvReloadTest, AsksConflictAfterFirstSave)
{
    // editing のフォルダが無い状態で開き、初回保存後でも、リモートの保存を検知して衝突を確認すること
    WriteAnalyzeResult(R"([{"original": "A", "type": ["name"]}])");
    ASSERT_FALSE(QFileInfo::exists(QFileInfo(editingCsv).absolutePath()));

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    const auto row = FindRow(*model, "A");
    const auto column = FindColumn(*model, "en");
    ASSERT_TRUE(row.has_value() && column.has_value());
    ASSERT_TRUE(model->setData(model->index(*row, *column), "Saved"));
    ASSERT_TRUE(manager->saveAllModels(component.setting->langscoreProjectDirectory + "/editing"));
    //保存で作ったフォルダとファイルの監視が始まるのを待つ
    WaitUntil([]() { return false; }, 1500);

    //ローカルでは保存せずに編集し、リモートで同じセルを保存する
    ASSERT_TRUE(model->setData(model->index(*row, *column), "Mine"));
    WriteTextFile(editingCsv, "original,en,type\nA,Theirs,name\n");

    //衝突のダイアログが表示されたら、「あとで決める」で閉じる
    QString theirs;
    EXPECT_TRUE(HandleConflictDialog([&theirs](QDialog& dialog, QTableWidget& conflicts) {
        theirs = conflicts.item(0, 3)->text();
        dialog.reject();
    })) << "衝突のダイアログが出ません";
    EXPECT_EQ(theirs, QString("Theirs"));
    EXPECT_EQ(FindCell(*model, "A", "en"), QString("Mine")) << "自分の訳が他の人の訳で上書きされています";
}

TEST_F(EditedCsvReloadTest, ConflictListChoosesSelectedRows)
{
    // 衝突を一覧表示し、選択した行をまとめてリモートの翻訳文にできること。未選択の行はローカルの翻訳文のまま
    WriteAnalyzeResult(R"([{"original": "A", "type": ["name"]}, {"original": "B", "type": ["name"]}, {"original": "C", "type": ["name"]}])");
    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenSavedActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    const auto column = *FindColumn(*model, "en");
    model->setData(model->index(*FindRow(*model, "A"), column), "MineA");
    model->setData(model->index(*FindRow(*model, "B"), column), "MineB");
    model->setData(model->index(*FindRow(*model, "C"), column), "MineC");
    WriteTextFile(editingCsv, "original,en,type\nA,TheirsA,name\nB,TheirsB,name\nC,TheirsC,name\n");

    int rowCount = 0;
    EXPECT_TRUE(HandleConflictDialog([&rowCount](QDialog& dialog, QTableWidget& conflicts) {
        rowCount = conflicts.rowCount();
        //A・B の行を選択し、まとめてリモートの翻訳文にする
        conflicts.clearSelection();
        conflicts.selectionModel()->select(conflicts.model()->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        conflicts.selectionModel()->select(conflicts.model()->index(1, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        dialog.findChild<QPushButton*>("selectedTheirsButton")->click();
        dialog.accept();
    })) << "衝突のダイアログが出ません";
    EXPECT_EQ(rowCount, 3);
    EXPECT_EQ(FindCell(*model, "A", "en"), QString("TheirsA"));
    EXPECT_EQ(FindCell(*model, "B", "en"), QString("TheirsB"));
    EXPECT_EQ(FindCell(*model, "C", "en"), QString("MineC"));
    QFile::remove(MainCSVTable::changeHistoryPath(component.setting->langscoreProjectDirectory));
}

TEST_F(EditedCsvReloadTest, ChangeByOthersIsHighlightedAndRecorded)
{
    // リモートの翻訳文を取り込んだセルをハイライトし、変更前後の翻訳文を履歴ファイルに残すこと。ローカルで編集するとハイライトを解除すること
    WriteAnalyzeResult(R"([{"original": "A", "type": ["name"]}, {"original": "B", "type": ["name"]}])");
    const auto historyPath = MainCSVTable::changeHistoryPath(component.setting->langscoreProjectDirectory);
    QFile::remove(historyPath);
    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    auto* model = OpenSavedActors(component, manager, table);
    ASSERT_NE(model, nullptr);

    WriteTextFile(editingCsv, "original,en,type\nA,TheirsA,name\nB,SavedB,name\n");
    ASSERT_TRUE(WaitUntil([model]() { return FindCell(*model, "A", "en") == QString("TheirsA"); }, 5000)) << "他の人の訳が入りません";

    const auto cellA = model->index(*FindRow(*model, "A"), *FindColumn(*model, "en"));
    const auto cellB = model->index(*FindRow(*model, "B"), *FindColumn(*model, "en"));
    EXPECT_TRUE(model->data(cellA, Qt::BackgroundRole).isValid()) << "他の人の訳にしたセルに色がありません";
    EXPECT_TRUE(model->data(cellA, Qt::ToolTipRole).toString().contains("SavedA"));
    EXPECT_FALSE(model->data(cellB, Qt::BackgroundRole).isValid());

    QFile history(historyPath);
    ASSERT_TRUE(history.open(QFile::ReadOnly)) << "履歴のファイルがありません";
    const auto lines = history.readAll().trimmed().split('\n');
    history.close();
    ASSERT_EQ(lines.size(), 1);
    const auto entry = QJsonDocument::fromJson(lines[0]).object();
    EXPECT_EQ(entry["file"].toString(), QString("Actors"));
    EXPECT_EQ(entry["original"].toString(), QString("A"));
    EXPECT_EQ(entry["language"].toString(), QString("en"));
    EXPECT_EQ(entry["before"].toString(), QString("SavedA"));
    EXPECT_EQ(entry["after"].toString(), QString("TheirsA"));

    model->setData(cellA, "MineA");
    EXPECT_FALSE(model->data(cellA, Qt::BackgroundRole).isValid()) << "自分で編集したセルの色が消えません";
    QFile::remove(historyPath);
}

TEST_F(EditedCsvReloadTest, NotifiesFileMarks)
{
    // ファイル一覧の状態表示として、未保存の編集・リモートの翻訳文の取り込み・未解決の衝突を通知すること
    WriteAnalyzeResult(R"([{"original": "A", "type": ["name"]}, {"original": "B", "type": ["name"]}])");
    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    struct Marks { bool modified = false; bool changedByOthers = false; bool hasConflicts = false; };
    Marks marks;
    QObject::connect(&table, &MainCSVTable::fileMarksChanged, [&marks](const QString& fileName, bool modified, bool changedByOthers, bool hasConflicts) {
        if(fileName == "Actors") { marks = {modified, changedByOthers, hasConflicts}; }
    });
    auto* model = OpenSavedActors(component, manager, table);
    ASSERT_NE(model, nullptr);
    EXPECT_FALSE(marks.modified);
    const auto column = *FindColumn(*model, "en");
    const auto cellA = model->index(*FindRow(*model, "A"), column);

    model->setData(cellA, "MineA");
    EXPECT_TRUE(marks.modified);

    //リモートで別のセルを変更する
    WriteTextFile(editingCsv, "original,en,type\nA,SavedA,name\nB,TheirsB,name\n");
    EXPECT_TRUE(WaitUntil([&marks]() { return marks.changedByOthers; }, 5000)) << "他の人の訳を取り込んだことを知らせていません";
    EXPECT_FALSE(marks.hasConflicts);

    //リモートで同じセルを変更し、あとで決める
    WriteTextFile(editingCsv, "original,en,type\nA,TheirsA,name\nB,TheirsB,name\n");
    EXPECT_TRUE(HandleConflictDialog([](QDialog& dialog, QTableWidget&) { dialog.reject(); }));
    EXPECT_TRUE(marks.hasConflicts) << "決めていない衝突を知らせていません";

    //保存前の確認で解決すると、衝突の状態表示が消える
    QTimer poll;
    QObject::connect(&poll, &QTimer::timeout, []() {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if(dialog != nullptr && dialog->objectName() == "mergeConflictDialog") { dialog->accept(); }
    });
    poll.start(50);
    table.syncWithAllFiles();
    EXPECT_FALSE(marks.hasConflicts);
    QFile::remove(MainCSVTable::changeHistoryPath(component.setting->langscoreProjectDirectory));
}

//-------------------------------------------------------------------
// 複数人での編集。各作業者が手元の PC のフォルダでプロジェクトを開き、
// Git の pull や、クラウドのドライブ（Google ドライブ・OneDrive・Dropbox など）の同期で、
// リモートで保存された editing の CSV が届く
//-------------------------------------------------------------------
namespace
{
    QByteArray ReadFile(const QString& path)
    {
        QFile file(path);
        if(file.open(QFile::ReadOnly) == false) { return {}; }
        return file.readAll();
    }

    //作業者1人分のアプリ。root の下の Game をローカルのプロジェクトとして開き、Actors を表示している
    struct Worker
    {
        QString root;
        ProjectUpdateTestComponent component;
        std::shared_ptr<EditDataManager> manager = std::make_shared<EditDataManager>();
        std::unique_ptr<MainCSVTable> table;
        MainCSVTableModel* model = nullptr;

        explicit Worker(const QString& root) : root(root) {}
        ~Worker()
        {
            //履歴は exe の隣のフォルダに残るため削除する
            QFile::remove(MainCSVTable::changeHistoryPath(component.setting->langscoreProjectDirectory));
        }

        QString editingFolder() const { return component.setting->langscoreProjectDirectory + "/editing"; }
        QString editingCsv() const { return editingFolder() + "/Actors.csv"; }

        //アプリを閉じる。未保存の編集は破棄する
        void close()
        {
            table.reset();
            model = nullptr;
            manager = std::make_shared<EditDataManager>();
        }

        //root の下のプロジェクトを開く。ファイルの監視が始まるまで待つ
        bool open()
        {
            auto& setting = *component.setting;
            setting.setGameProjectPath(root + "/Game");
            setting.load(setting.langscoreProjectDirectory + "/config.json");
            SetLanguages(setting, {{"en", true}});
            table = std::make_unique<MainCSVTable>(&component, manager);
            model = OpenActors(component, manager, *table);
            WaitUntil([]() { return false; }, 1000);
            return model != nullptr;
        }

        QModelIndex cell(const QString& original) const
        {
            return model->index(static_cast<int>(FindRow(*model, original).value()), static_cast<int>(FindColumn(*model, "en").value()));
        }
        void edit(const QString& original, const QString& text) { model->setData(cell(original), text); }
        QString text(const QString& original) const { return FindCell(*model, original, "en").value_or("<no cell>"); }
        bool highlighted(const QString& original) const { return model->data(cell(original), Qt::BackgroundRole).isValid(); }
        //保存した直後のファイルをウイルス対策ソフトなどが開いていると、置き換えに失敗する場合があるため、少し待って再試行する
        bool save() { return WaitUntil([this]() { return manager->saveAllModels(editingFolder()); }, 3000); }

        //リモートの翻訳文を取り込んだ履歴
        QList<QJsonObject> history() const
        {
            QList<QJsonObject> entries;
            for(const auto& line : ReadFile(MainCSVTable::changeHistoryPath(component.setting->langscoreProjectDirectory)).split('\n'))
            {
                if(line.trimmed().isEmpty()) { continue; }
                entries.append(QJsonDocument::fromJson(line).object());
            }
            return entries;
        }
    };

    //原文が A・B・C の3行の解析結果
    const QByteArray SharedActors = R"([{"original": "A", "type": ["name"]}, {"original": "B", "type": ["name"]}, {"original": "C", "type": ["name"]}])";

    //owner がプロジェクトを作成し、A の行のみ翻訳して保存する。actors は Actors の解析結果で、A の行を含める
    void CreateSharedProject(Worker& owner, const QByteArray& actors = SharedActors)
    {
        SetupProject(owner.component, owner.root + "/Game");
        WriteTextFile(owner.component.setting->analyzeDirectoryPath() + "/Actors.lsjson", actors);
        ASSERT_TRUE(owner.open());
        owner.edit("A", "SavedA");
        ASSERT_TRUE(owner.save());
    }

    //衝突のダイアログが表示されたら数え、handle で操作する。既定では「あとで決める」で閉じる
    //表示されないはずの場面や、プロジェクトを開いている途中に表示される場面で、テストが止まらないようにする
    struct ConflictDialogCounter
    {
        using Handle = std::function<void(QDialog&, QTableWidget&)>;
        int count = 0;
        QTimer poll;

        explicit ConflictDialogCounter(Handle handle = [](QDialog& dialog, QTableWidget&) { dialog.reject(); })
        {
            QObject::connect(&poll, &QTimer::timeout, [this, handle]() {
                auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
                if(dialog == nullptr || dialog->objectName() != "mergeConflictDialog") { return; }
                auto* table = dialog->findChild<QTableWidget*>("conflictTable");
                if(table == nullptr) { return; }
                ++count;
                handle(*dialog, *table);
            });
            poll.start(50);
        }
    };

    //衝突の一覧の各行を「原文:ローカルの翻訳文/リモートの翻訳文」の形式にする
    QStringList ConflictRows(const QTableWidget& table)
    {
        QStringList rows;
        for(int row = 0; row < table.rowCount(); ++row) {
            rows.append(table.item(row, 0)->text() + ":" + table.item(row, 2)->text() + "/" + table.item(row, 3)->text());
        }
        return rows;
    }

    //Git で共有する。A が作成したプロジェクトを push し、B が clone して、両者ともアプリで開いている
    class GitSharedEditingTest : public ::testing::Test
    {
    protected:
        QTemporaryDir tempDir;
        Worker a{tempDir.filePath("A")};
        Worker b{tempDir.filePath("B")};

        void SetUp() override
        {
            if(QStandardPaths::findExecutable("git").isEmpty()) { GTEST_SKIP() << "git が見つかりません"; }
            WriteTextFile(tempDir.filePath("gitconfig"), "");
            ASSERT_TRUE(git(tempDir.path(), {"init", "--bare", "-b", "main", "remote.git"}));
            CreateSharedProject(a, actors());
            if(HasFatalFailure()) { return; }
            ASSERT_TRUE(git(a.root, {"init", "-b", "main"}));
            ASSERT_TRUE(git(a.root, {"remote", "add", "origin", tempDir.filePath("remote.git")}));
            ASSERT_TRUE(push(a, "A を訳した"));
            ASSERT_TRUE(git(tempDir.path(), {"clone", tempDir.filePath("remote.git"), "B"}));
            ASSERT_TRUE(b.open());
        }

        //Actors の解析結果
        virtual QByteArray actors() const { return SharedActors; }

        //git を実行する。改行の変換など、この PC の git の設定に左右されないよう、全体の設定は読まない
        bool git(const QString& folder, const QStringList& arguments, QString* output = nullptr)
        {
            auto environment = QProcessEnvironment::systemEnvironment();
            environment.insert("GIT_CONFIG_NOSYSTEM", "1");
            environment.insert("GIT_CONFIG_GLOBAL", tempDir.filePath("gitconfig"));
            environment.insert("GIT_AUTHOR_NAME", "langscore-test");
            environment.insert("GIT_AUTHOR_EMAIL", "langscore-test@example.com");
            environment.insert("GIT_COMMITTER_NAME", "langscore-test");
            environment.insert("GIT_COMMITTER_EMAIL", "langscore-test@example.com");
            QProcess process;
            process.setProcessEnvironment(environment);
            process.setWorkingDirectory(folder);
            process.setProcessChannelMode(QProcess::MergedChannels);
            process.start("git", arguments);
            const bool finished = process.waitForFinished(30000);
            const auto text = QString::fromUtf8(process.readAll());
            if(output != nullptr) { *output = text; }
            const bool succeeded = finished && process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
            if(succeeded == false && output == nullptr) {
                ADD_FAILURE() << "git " << arguments.join(' ').toStdString() << "\n" << text.toStdString();
            }
            return succeeded;
        }

        bool commit(const Worker& worker, const QString& message)
        {
            return git(worker.root, {"add", "-A"}) && git(worker.root, {"commit", "-m", message});
        }
        bool push(const Worker& worker, const QString& message)
        {
            return commit(worker, message) && git(worker.root, {"push", "origin", "main"});
        }
        bool pull(const Worker& worker, QString* output = nullptr)
        {
            return git(worker.root, {"pull", "--no-rebase", "--no-edit", "origin", "main"}, output);
        }
    };

    //クラウドのドライブで共有する。両者の PC にそれぞれ同期されたフォルダがあり、両者ともアプリで開いている
    class CloudSharedEditingTest : public ::testing::Test
    {
    protected:
        QTemporaryDir tempDir;
        Worker a{tempDir.filePath("A")};
        Worker b{tempDir.filePath("B")};

        void SetUp() override
        {
            CreateSharedProject(a);
            if(HasFatalFailure()) { return; }
            ASSERT_FALSE(CopyDirectory(a.root, b.root).isEmpty());
            ASSERT_TRUE(b.open());
        }

        //同期ソフトと同様に、届いたファイルを一時ファイルに受け取ってから置き換える。
        //更新日時は、保存した作業者の PC での日時のままにする。置き換えるファイルを他で開いていたら、少し待って再試行する
        static bool SyncFile(const QString& source, const QString& target)
        {
            const auto temp = QFileInfo(target).absolutePath() + "/.~" + QFileInfo(target).fileName() + ".tmp";
            QFile::remove(temp);
            if(QFile::copy(source, temp) == false) { return false; }
            {
                QFile file(temp);
                if(file.open(QFile::ReadWrite) == false) { return false; }
                if(file.setFileTime(QFileInfo(source).lastModified(), QFileDevice::FileModificationTime) == false) { return false; }
            }
            std::error_code error;
            const bool renamed = WaitUntil([&]() {
                std::filesystem::rename(temp.toStdWString(), target.toStdWString(), error);
                return static_cast<bool>(error) == false;
            }, 3000);
            if(renamed == false) { ADD_FAILURE() << "同期できません: " << error.message(); }
            return renamed;
        }

        //from の作業者が保存した Actors.csv を、to の作業者の PC に同期する
        static bool sync(const Worker& from, const Worker& to) { return SyncFile(from.editingCsv(), to.editingCsv()); }
    };
}

TEST_F(GitSharedEditingTest, PullTakesOthersCommit)
{
    // リモートで push された翻訳文を pull すると、再保存しなくても表に反映され、取り込んだセルがハイライトされること
    ConflictDialogCounter dialogs;
    b.edit("B", "TheirsB");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "B を訳した"));

    ASSERT_TRUE(pull(a));
    EXPECT_TRUE(WaitUntil([this]() { return a.text("B") == "TheirsB"; }, 5000)) << "pull した訳が表に入りません";
    EXPECT_EQ(a.text("A"), QString("SavedA"));
    EXPECT_TRUE(a.highlighted("B"));
    EXPECT_FALSE(a.highlighted("A"));
    EXPECT_FALSE(a.model->isModified()) << "pull しただけで未保存の扱いになっています";
    EXPECT_EQ(dialogs.count, 0);
}

TEST_F(GitSharedEditingTest, PullMergesOthersCommitWithMine)
{
    // ローカルでも別の行を commit していて、Git が pull で両者の変更をマージしても、両方の翻訳文が残ること。
    // ローカルの未保存の編集も残ること
    ConflictDialogCounter dialogs;
    b.edit("C", "TheirsC");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "C を訳した"));

    a.edit("A", "MineA");
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(commit(a, "A を直した"));
    a.edit("B", "UnsavedB");
    QString output;
    ASSERT_TRUE(pull(a, &output)) << output.toStdString();

    EXPECT_TRUE(WaitUntil([this]() { return a.text("C") == "TheirsC"; }, 5000)) << "pull した訳が表に入りません";
    EXPECT_EQ(a.text("A"), QString("MineA"));
    EXPECT_EQ(a.text("B"), QString("UnsavedB"));

    //保存して push すると、B も pull で A の翻訳文を受け取れる
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(push(a, "B を訳した"));
    ASSERT_TRUE(pull(b));
    EXPECT_TRUE(WaitUntil([this]() { return b.text("B") == "UnsavedB"; }, 5000)) << "B に A の訳が届きません";
    EXPECT_EQ(b.text("A"), QString("MineA"));
    EXPECT_EQ(b.text("C"), QString("TheirsC"));
    EXPECT_EQ(dialogs.count, 0);
}

TEST_F(GitSharedEditingTest, PullAsksWhenSameCellIsEditedUnsaved)
{
    // ローカルで未保存のセルをリモートでも変更して push していた場合、pull 時にどちらの翻訳文にするか確認すること。
    // ローカルの翻訳文を選ぶと保存して push でき、リモートにもローカルの翻訳文が届く
    b.edit("A", "TheirsA");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "A を直した"));

    a.edit("A", "MineA");
    ASSERT_TRUE(pull(a));
    QString theirs;
    EXPECT_TRUE(HandleConflictDialog([&theirs](QDialog& dialog, QTableWidget& conflicts) {
        theirs = conflicts.item(0, 3)->text();
        dialog.accept();
    })) << "衝突のダイアログが出ません";
    EXPECT_EQ(theirs, QString("TheirsA"));
    EXPECT_EQ(a.text("A"), QString("MineA"));

    ASSERT_TRUE(a.save());
    ASSERT_TRUE(push(a, "A を決めた"));
    ASSERT_TRUE(pull(b));
    EXPECT_TRUE(WaitUntil([this]() { return b.text("A") == "MineA"; }, 5000)) << "B に A の訳が届きません";
}

TEST_F(GitSharedEditingTest, MergeConflictIsResolvedInApp)
{
    // 両者が同じ行を commit して pull で Git の競合になったら、競合マーカー内の両者の翻訳文を、アプリの衝突として確認すること。
    // 選択どおりに保存すると競合マーカーが消え、そのまま commit して push できる
    b.edit("A", "TheirsA");
    b.edit("C", "TheirsC");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "A と C を訳した"));

    a.edit("A", "MineA");
    a.edit("C", "MineC");
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(commit(a, "A と C を訳した"));
    QString output;
    ASSERT_FALSE(pull(a, &output)) << "Git の競合になりません\n" << output.toStdString();
    ASSERT_TRUE(ReadFile(a.editingCsv()).contains("<<<<<<<"));

    //A はリモートの翻訳文、C はローカルの翻訳文にする
    QStringList conflicts;
    EXPECT_TRUE(HandleConflictDialog([&conflicts](QDialog& dialog, QTableWidget& table) {
        conflicts = ConflictRows(table);
        table.clearSelection();
        for(int row = 0; row < table.rowCount(); ++row)
        {
            if(table.item(row, 0)->text() != "A") { continue; }
            table.selectionModel()->select(table.model()->index(row, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        }
        dialog.findChild<QPushButton*>("selectedTheirsButton")->click();
        dialog.accept();
    })) << "競合のダイアログが出ません";
    EXPECT_EQ(conflicts, QStringList({"A:MineA/TheirsA", "C:MineC/TheirsC"}));
    EXPECT_EQ(a.text("A"), QString("TheirsA"));
    EXPECT_EQ(a.text("C"), QString("MineC"));

    ASSERT_TRUE(a.save());
    const auto saved = ReadFile(a.editingCsv());
    EXPECT_FALSE(saved.contains("<<<<<<<") || saved.contains("=======") || saved.contains(">>>>>>>")) << "保存したファイルに競合の印が残っています\n" << saved.toStdString();
    ASSERT_TRUE(push(a, "競合を解決した"));
    ASSERT_TRUE(pull(b));
    EXPECT_TRUE(WaitUntil([this]() { return b.text("C") == "MineC"; }, 5000)) << "B に A の訳が届きません";
    EXPECT_EQ(b.text("A"), QString("TheirsA"));
}

TEST_F(GitSharedEditingTest, OpeningConflictedFileAsks)
{
    // アプリを閉じている間に pull で Git の競合になっても、開いたときに競合マーカー内の両者の翻訳文を衝突として確認すること。
    // 解決するまでの表はローカルの翻訳文のままで、書き出し・検証など他の処理が読むときもローカル側の内容になる
    b.edit("A", "TheirsA");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "A を直した"));

    a.edit("A", "MineA");
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(commit(a, "A を直した"));
    a.close();
    QString output;
    ASSERT_FALSE(pull(a, &output)) << "Git の競合になりません\n" << output.toStdString();

    QStringList conflicts;
    ConflictDialogCounter dialogs([&conflicts](QDialog& dialog, QTableWidget& table) {
        conflicts = ConflictRows(table);
        dialog.reject();
    });
    ASSERT_TRUE(a.open());
    EXPECT_TRUE(WaitUntil([&dialogs]() { return dialogs.count > 0; }, 5000)) << "競合のダイアログが出ません";
    EXPECT_EQ(conflicts, QStringList({"A:MineA/TheirsA"}));
    EXPECT_EQ(a.text("A"), QString("MineA"));

    langscore::FastCSVContainer ours;
    ASSERT_TRUE(ours.loadFromFile(a.editingCsv()));
    EXPECT_EQ(ours.rowCount(), size_t{3}) << "競合の印が行になっています";
    EXPECT_EQ(ours.at(0, QString("en")), QString("MineA"));
}

TEST_F(GitSharedEditingTest, LineEndingConversionIsNotAChange)
{
    // core.autocrlf で Git が改行を変換しても、未変更のセルをリモートの変更として扱わないこと。セル内の改行も同様
    ConflictDialogCounter dialogs;
    ASSERT_TRUE(git(b.root, {"config", "core.autocrlf", "true"}));
    a.edit("A", "Line1\nLine2");
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(push(a, "A を2行にした"));

    ASSERT_TRUE(pull(b));
    ASSERT_TRUE(ReadFile(b.editingCsv()).contains("\r\n")) << "Git が改行を変換していません";
    EXPECT_TRUE(WaitUntil([this]() { return b.text("A") == "Line1\nLine2"; }, 5000)) << "pull した訳が表に入りません";
    EXPECT_FALSE(b.highlighted("B"));
    EXPECT_EQ(b.history().size(), 1) << "改行が変わっただけのセルを、他の人の変更として扱っています";

    b.edit("C", "TheirsC");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "C を訳した"));
    ASSERT_TRUE(pull(a));
    EXPECT_TRUE(WaitUntil([this]() { return a.text("C") == "TheirsC"; }, 5000)) << "pull した訳が表に入りません";
    EXPECT_EQ(a.text("A"), QString("Line1\nLine2"));
    EXPECT_FALSE(a.highlighted("A")) << "改行が変わっただけのセルを、他の人の変更として扱っています";
    EXPECT_EQ(a.history().size(), 1);
    EXPECT_EQ(dialogs.count, 0);
}

namespace
{
    //実際の翻訳文に近い、読み違えやすい文章で Git の競合を起こす。原文にも改行・引用符・制御文字を含める
    class GitComplexTextTest : public GitSharedEditingTest
    {
    protected:
        QByteArray actors() const override
        {
            return R"json([
                {"original": "A", "type": ["name"]},
                {"original": "複数行の\n原文", "type": ["message"]},
                {"original": "Say \"Hi\", friend", "type": ["message"]},
                {"original": "\\C[2]エリック\\C[0]", "type": ["message"]},
                {"original": "区切り", "type": ["message"]},
                {"original": "絵文字", "type": ["message"]},
                {"original": "消す", "type": ["message"]},
                {"original": "隣", "type": ["message"]}
            ])json";
        }
    };

    struct ConflictedText
    {
        QString original;
        QString base;   //base の翻訳文
        QString theirs;
        QString mine;
        bool useTheirs; //衝突の一覧でリモートの翻訳文を選ぶ
        QString chosen() const { return useTheirs ? theirs : mine; }
    };
}

TEST_F(GitComplexTextTest, ComplexTextsAreAskedAndSavedExactly)
{
    // 改行・引用符・カンマ・制御文字・絵文字・タブ・前後の空白・「=======」のみの行・空の翻訳文で Git の競合になっても、
    // 両者の翻訳文をそのまま確認し、選択どおりに保存できること
    const std::vector<ConflictedText> cells = {
        //セルの途中の行だけが競合する
        {"複数行の\n原文", "First line\nSecond line\nThird line", "First line\nTheirs second\nThird line", "First line\nMine second\nThird line", true},
        {"Say \"Hi\", friend", "Base, \"quoted\"", "\"Theirs\", with, commas", "\"Mine\" said \"no\"", false},
        {"\\C[2]エリック\\C[0]", "\\C[2]Eric\\C[0]", "\\C[2]Erik\\C[0]\\!\\|\\", "\\C[3]エリク\\C[0]\\.", true},
        //競合の区切りと同じ行を含む
        {"区切り", "Base", "Top\n=======\nTheirs bottom", "Mine top\n=======\nBottom", false},
        {"絵文字", "Base", "  😀 Theirs\tタブ  ", "  😀 Mine\tタブ  ", true},
        {"消す", "Base", "", "Mine", true},
    };

    for(const auto& cell : cells) { a.edit(cell.original, cell.base); }
    a.edit("隣", "Base");
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(push(a, "元の訳"));
    ASSERT_TRUE(pull(b));
    ASSERT_TRUE(WaitUntil([this]() { return b.text("隣") == "Base"; }, 5000)) << "B に元の訳が届きません";
    for(const auto& cell : cells) { ASSERT_EQ(b.text(cell.original), cell.base) << cell.original.toStdString(); }

    for(const auto& cell : cells) { b.edit(cell.original, cell.theirs); }
    b.edit("隣", "Theirs neighbor");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(push(b, "他の人の訳"));

    for(const auto& cell : cells) { a.edit(cell.original, cell.mine); }
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(commit(a, "自分の訳"));
    QString output;
    ASSERT_FALSE(pull(a, &output)) << "Git の競合になりません\n" << output.toStdString();

    QStringList asked;
    EXPECT_TRUE(HandleConflictDialog([&asked, &cells](QDialog& dialog, QTableWidget& table) {
        asked = ConflictRows(table);
        for(int row = 0; row < table.rowCount(); ++row)
        {
            for(const auto& cell : cells)
            {
                if(table.item(row, 0)->text() != cell.original || cell.useTheirs == false) { continue; }
                qobject_cast<QComboBox*>(table.cellWidget(row, 4))->setCurrentIndex(1);
            }
        }
        dialog.accept();
    })) << "競合のダイアログが出ません";
    QStringList expected;
    for(const auto& cell : cells) { expected.append(cell.original + ":" + cell.mine + "/" + cell.theirs); }
    EXPECT_EQ(asked, expected);
    for(const auto& cell : cells) { EXPECT_EQ(a.text(cell.original), cell.chosen()) << cell.original.toStdString(); }
    EXPECT_EQ(a.text("隣"), QString("Theirs neighbor")) << "他の人だけが変えた訳を取り込んでいません";

    ASSERT_TRUE(a.save());
    langscore::GitConflictSides sides;
    ASSERT_TRUE(langscore::FastCSVContainer::loadGitConflictSidesFromFile(a.editingCsv(), sides));
    EXPECT_FALSE(sides.hasConflictMarkers) << "保存したファイルに競合の印が残っています\n" << ReadFile(a.editingCsv()).toStdString();
    const auto saved = langscore::CSVEditorTableModel(a.editingCsv());
    for(const auto& cell : cells) { EXPECT_EQ(FindCell(saved, cell.original, "en"), cell.chosen()) << cell.original.toStdString(); }

    ASSERT_TRUE(push(a, "競合を解決した"));
    ASSERT_TRUE(pull(b));
    EXPECT_TRUE(WaitUntil([this, &cells]() {
        return std::all_of(cells.begin(), cells.end(), [this](const ConflictedText& cell) { return b.text(cell.original) == cell.chosen(); });
    }, 5000)) << "B に解決した訳が届きません";
    for(const auto& cell : cells) { EXPECT_EQ(b.text(cell.original), cell.chosen()) << cell.original.toStdString(); }
    EXPECT_EQ(b.text("隣"), QString("Theirs neighbor"));
}

TEST_F(CloudSharedEditingTest, SyncedSaveIsTakenBothWays)
{
    // リモートの保存が同期されると表に反映され、ローカルの保存も同期でリモートに届くこと。互いの翻訳文が両方に残る
    ConflictDialogCounter dialogs;
    b.edit("B", "TheirsB");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(sync(b, a));
    EXPECT_TRUE(WaitUntil([this]() { return a.text("B") == "TheirsB"; }, 5000)) << "同期された訳が表に入りません";
    EXPECT_TRUE(a.highlighted("B"));

    a.edit("C", "MineC");
    ASSERT_TRUE(a.save());
    ASSERT_TRUE(sync(a, b));
    EXPECT_TRUE(WaitUntil([this]() { return b.text("C") == "MineC"; }, 5000)) << "B に A の訳が届きません";
    EXPECT_EQ(b.text("A"), QString("SavedA"));
    EXPECT_EQ(b.text("B"), QString("TheirsB"));
    EXPECT_EQ(a.text("B"), QString("TheirsB"));
    EXPECT_EQ(dialogs.count, 0);
}

TEST_F(CloudSharedEditingTest, OldTimestampIsTaken)
{
    // PC の時計のずれや同期の遅れで、同期されたファイルの更新日時が手元のファイルより古くても取り込むこと
    ConflictDialogCounter dialogs;
    b.edit("B", "TheirsB");
    ASSERT_TRUE(b.save());
    {
        QFile file(b.editingCsv());
        ASSERT_TRUE(file.open(QFile::ReadWrite));
        ASSERT_TRUE(file.setFileTime(QDateTime::currentDateTime().addDays(-1), QFileDevice::FileModificationTime));
    }
    ASSERT_LT(QFileInfo(b.editingCsv()).lastModified(), QFileInfo(a.editingCsv()).lastModified());

    ASSERT_TRUE(sync(b, a));
    EXPECT_TRUE(WaitUntil([this]() { return a.text("B") == "TheirsB"; }, 5000)) << "日時の古いファイルを取り込んでいません";
    EXPECT_EQ(dialogs.count, 0);
}

TEST_F(CloudSharedEditingTest, ConflictCopyIsIgnored)
{
    // 両者がほぼ同時に保存すると、同期ソフトは片方を「Actors (1).csv」などの別のファイルにする。
    // そのファイルを Actors の翻訳文として読まず、ローカルの保存で書き換えないこと
    ConflictDialogCounter dialogs;
    b.edit("A", "TheirsA");
    ASSERT_TRUE(b.save());
    const QStringList copies = {"Actors (1).csv", "Actors (B's conflicted copy).csv", "Actors-PC-B.csv"};
    for(const auto& name : copies) {
        ASSERT_TRUE(SyncFile(b.editingCsv(), a.editingFolder() + "/" + name));
    }
    WaitUntil([]() { return false; }, 1500);
    EXPECT_EQ(a.text("A"), QString("SavedA")) << "競合したときに作られたファイルから訳を取り込んでいます";
    EXPECT_FALSE(a.highlighted("A"));

    a.edit("C", "MineC");
    ASSERT_TRUE(a.save());
    for(const auto& name : copies) {
        EXPECT_EQ(ReadFile(a.editingFolder() + "/" + name), ReadFile(b.editingCsv())) << name.toStdString();
    }
    EXPECT_EQ(dialogs.count, 0);
}

TEST_F(CloudSharedEditingTest, StaleCopyDoesNotSilentlyDropMine)
{
    // ローカルの保存がリモートに同期される前にリモートでも保存すると、リモートのファイルにはローカルの翻訳文が無い。
    // それが同期されても、ローカルの翻訳文が通知なく消えないこと。翻訳文を残すか、ハイライトと履歴で元の翻訳文が分かること
    ConflictDialogCounter dialogs;
    a.edit("A", "MineA");
    ASSERT_TRUE(a.save());
    b.edit("B", "TheirsB");
    ASSERT_TRUE(b.save());
    ASSERT_TRUE(sync(b, a));
    EXPECT_TRUE(WaitUntil([this]() { return a.text("B") == "TheirsB"; }, 5000)) << "同期された訳が表に入りません";

    if(a.text("A") == "MineA") { return; }
    EXPECT_TRUE(a.highlighted("A")) << "自分の訳が、色も付かずに古い訳に戻っています";
    const auto history = a.history();
    const bool recorded = std::ranges::any_of(history, [](const QJsonObject& entry) {
        return entry["original"].toString() == "A" && entry["before"].toString() == "MineA";
    });
    EXPECT_TRUE(recorded) << "消えた自分の訳が履歴にありません";
}

#ifdef Q_OS_WIN
TEST_F(CloudSharedEditingTest, FileLockedWhileSyncingIsReadLater)
{
    // 同期ソフトがファイルに書き込み中の間はアプリから読み込めない。書き込み完了後に取り込むこと
    ConflictDialogCounter dialogs;
    b.edit("B", "TheirsB");
    ASSERT_TRUE(b.save());
    const auto content = ReadFile(b.editingCsv());

    //他から開けないようにして、手元のファイルに書き込む
    HANDLE file = CreateFileW(a.editingCsv().toStdWString().c_str(), GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    ASSERT_NE(file, INVALID_HANDLE_VALUE);
    DWORD written = 0;
    WriteFile(file, content.constData(), static_cast<DWORD>(content.size()), &written, nullptr);
    //変更から 0.5 秒後にアプリが読み込む間も開いたままにする
    WaitUntil([]() { return false; }, 1500);
    CloseHandle(file);

    EXPECT_TRUE(WaitUntil([this]() { return a.text("B") == "TheirsB"; }, 5000)) << "書き終わった後のファイルを読み直していません";
    EXPECT_EQ(dialogs.count, 0);
}
#endif
