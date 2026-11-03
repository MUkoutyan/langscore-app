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

// setting を参照するために公開する
struct ProjectUpdateTestComponent : public ComponentBase
{
    using ComponentBase::setting;
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
TEST(ProjectUpdateTest, UpdateReloadsFileList) // [25]
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

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST(ProjectUpdateTest, EditedCsvIncludesNewTexts) // [26]
{
    // editing に保存した CSV があっても、更新で増えた文章が表に出て、保存した翻訳は残ること
    QTemporaryDir tempDir;
    ProjectUpdateTestComponent component;
    SetupProject(component, tempDir.filePath("Game"));
    const auto& setting = *component.setting;

    //更新後の解析結果には B が増えている
    WriteTextFile(setting.analyzeDirectoryPath() + "/Actors.lsjson", R"([
        {"original": "A", "type": ["name"]},
        {"original": "B", "type": ["name"]}
    ])");
    //更新前に A だけを翻訳して保存した CSV
    const auto editingCsv = setting.langscoreProjectDirectory + "/editing/Actors.csv";
    WriteTextFile(editingCsv, "original,en,type\nA,Hello,name\n");

    auto manager = std::make_shared<EditDataManager>();
    MainCSVTable table(&component, manager);
    table.showMainFileText("Actors", "Actors");

    auto* model = manager->getMainCSVModel(editingCsv);
    ASSERT_NE(model, nullptr);
    std::optional<size_t> rowA;
    std::optional<size_t> rowB;
    for(size_t row = 0; row < static_cast<size_t>(model->rowCount()); ++row)
    {
        const auto original = model->getCell(row, 0).value_or("");
        if(original == "A") { rowA = row; }
        if(original == "B") { rowB = row; }
    }
    EXPECT_TRUE(rowB.has_value()) << "更新で増えた文章 B が表にありません";
    ASSERT_TRUE(rowA.has_value());
    EXPECT_EQ(model->getCell(*rowA, 1).value_or(""), QString("Hello")) << "保存した翻訳が失われています";
}
