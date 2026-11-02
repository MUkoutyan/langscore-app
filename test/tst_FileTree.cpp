#include "test_common.h"
#include <QTreeWidget>
#include <QLineEdit>
#include "FileTree.h"
#include "EditDataManager.h"

// FileTree.cpp の UserRole::VISIBLE と同じ値
constexpr int VisibleRole = Qt::UserRole + 4;

// runtimeData を参照するために公開する
struct TestComponent : public ComponentBase
{
    using ComponentBase::runtimeData;
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
