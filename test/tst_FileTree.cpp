#include "test_common.h"
#include <QTreeWidget>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QImage>
#include "FileTree.h"
#include "EditDataManager.h"

// FileTree.cpp の UserRole::VISIBLE と同じ値
constexpr int VisibleRole = Qt::UserRole + 4;

// runtimeData を参照するために公開する
struct TestComponent : public ComponentBase
{
    using ComponentBase::runtimeData;
    using ComponentBase::setting;
};

class FileTreeTest : public ::testing::Test
{
protected:
    std::unique_ptr<TestComponent> component;
    std::shared_ptr<EditDataManager> manager;
    std::unique_ptr<FileTree> tree;

    void SetUp() override
    {
        component = std::make_unique<TestComponent>();
        manager = std::make_shared<EditDataManager>();
        tree = std::make_unique<FileTree>(component.get(), manager);
    }

    void TearDown() override
    {
        tree.reset();
        manager.reset();
        component.reset();
    }
};

// 不具合の再発防止
TEST_F(FileTreeTest, SearchShowsVisibleItems)
{
    // 検索時、表示設定の項目は一致すれば表示され、非表示設定の項目は隠れること
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    auto* searchEdit = tree->findChild<QLineEdit*>();
    ASSERT_NE(treeWidget, nullptr);
    ASSERT_NE(searchEdit, nullptr);

    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    auto* visibleItem = new QTreeWidgetItem(top, {"Actors"});
    visibleItem->setData(0, VisibleRole, true);
    auto* hiddenItem = new QTreeWidgetItem(top, {"ActorsHidden"});
    hiddenItem->setData(0, VisibleRole, false);

    searchEdit->setText("Actor");

    EXPECT_FALSE(visibleItem->isHidden());
    EXPECT_TRUE(hiddenItem->isHidden());
}

TEST_F(FileTreeTest, ErrorJsonSplitAcrossChunks)
{
    // divisi の標準出力が JSON の途中で分割されて届いても、エラーが失われないこと
    const QString json = R"({"Type":0,"Summary":0,"File":"C:/data/Actors.csv","Row":3,"Language":"en","Details":""})";
    const auto half = json.size() / 2;
    ASSERT_TRUE(QMetaObject::invokeMethod(tree.get(), "addErrorText", Q_ARG(QString, json.left(half))));
    ASSERT_TRUE(QMetaObject::invokeMethod(tree.get(), "addErrorText", Q_ARG(QString, json.mid(half))));

    const auto& errors = component->runtimeData->errors;
    ASSERT_TRUE(errors.contains("Actors"));
    ASSERT_EQ(errors.at("Actors").size(), size_t{1});
    EXPECT_EQ(errors.at("Actors")[0].row, std::int64_t{3});
}

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(FileTreeTest, IgnorePictureWrittenInDivisiFormat) // [19]
{
    // 画像のチェックを外すと、divisi が比較する形式（ゲームフォルダからの相対パス・拡張子なし）で除外リストに入ること
    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    QDir().mkpath(gameDir + "/img/faces");
    QFile projectFile(gameDir + "/Game.rmmzproject");
    ASSERT_TRUE(projectFile.open(QFile::WriteOnly));
    projectFile.close();
    QImage image(1, 1, QImage::Format_RGB32);
    image.fill(Qt::white);
    ASSERT_TRUE(image.save(gameDir + "/img/faces/Actor1.png"));

    //画像一覧は設定の読み込み時に作られる
    auto& setting = *component->setting;
    setting.setGameProjectPath(gameDir);
    const auto configPath = setting.langscoreProjectDirectory + "/config.json";
    setting.write(configPath);
    setting.load(configPath);
    tree->setupGraphicsTree();

    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    const auto found = treeWidget->findItems("Actor1.png", Qt::MatchExactly | Qt::MatchRecursive, 0);
    ASSERT_EQ(found.size(), 1);
    found[0]->setCheckState(0, Qt::Unchecked);

    EXPECT_EQ(setting.writeObj.ignorePicturePath, std::vector<QString>{"img/faces/Actor1"});
}
