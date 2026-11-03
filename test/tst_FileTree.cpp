#include "test_common.h"
#include <QTreeWidget>
#include <QLineEdit>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QImage>
#include "FileTree.h"
#include "EditDataManager.h"

// FileTree.cpp の UserRole::FILE_NAME / VISIBLE と同じ値
constexpr int FileNameRole = Qt::UserRole + 2;
constexpr int VisibleRole = Qt::UserRole + 4;

// runtimeData を参照するために公開する
struct TestComponent : public ComponentBase
{
    using ComponentBase::runtimeData;
    using ComponentBase::setting;
    using ComponentBase::addDispatch;
    using ComponentBase::dispatch;
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

TEST_F(FileTreeTest, ValidationErrorShownOnFileWithExtension)
{
    // 検証結果は拡張子なしの名前（Actors）で届くが、拡張子付きの項目（Actors.json）に表示されること。
    // 次の検証を始めたら、前回の表示は消えること
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    ASSERT_NE(treeWidget, nullptr);
    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    auto* actors = new QTreeWidgetItem(top, {"Actors"});
    actors->setData(0, FileNameRole, "Actors.json");

    ValidationErrorInfo info;
    info.type = ValidationErrorInfo::Error;
    info.summary = ValidationErrorInfo::EmptyCol;
    info.row = 3;
    info.language = "en";
    component->runtimeData->errors["Actors"].push_back(info);
    component->runtimeData->updateList["Actors"] = true;

    component->addDispatch(tree.get());
    component->dispatch(ComponentBase::NotifyFinishValidateCSV, {});
    EXPECT_EQ(actors->childCount(), 1) << "Actors.json の項目に検証結果が表示されていません";

    component->dispatch(ComponentBase::ValidateCSV, {});
    EXPECT_EQ(actors->childCount(), 0) << "次の検証を始めても前回の検証結果が残っています";
}

TEST_F(FileTreeTest, IgnorePictureWrittenInDivisiFormat)
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
