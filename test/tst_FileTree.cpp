#include <QTest>
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

class TestFileTree : public QObject
{
    Q_OBJECT

private slots:
    void init();
    void cleanup();

    // 不具合の再発防止
    void testSearchShowsVisibleItems();
    void testErrorJsonSplitAcrossChunks();

private:
    std::unique_ptr<TestComponent> component;
    std::shared_ptr<EditDataManager> manager;
    std::unique_ptr<FileTree> tree;
};

void TestFileTree::init()
{
    component = std::make_unique<TestComponent>();
    manager = std::make_shared<EditDataManager>();
    tree = std::make_unique<FileTree>(component.get(), manager);
}

void TestFileTree::cleanup()
{
    tree.reset();
    manager.reset();
    component.reset();
}

void TestFileTree::testSearchShowsVisibleItems()
{
    // 検索時、表示設定の項目は一致すれば表示され、非表示設定の項目は隠れること
    auto* treeWidget = tree->findChild<QTreeWidget*>();
    auto* searchEdit = tree->findChild<QLineEdit*>();
    QVERIFY(treeWidget != nullptr);
    QVERIFY(searchEdit != nullptr);

    auto* top = new QTreeWidgetItem(treeWidget, {"Basics"});
    auto* visibleItem = new QTreeWidgetItem(top, {"Actors"});
    visibleItem->setData(0, VisibleRole, true);
    auto* hiddenItem = new QTreeWidgetItem(top, {"ActorsHidden"});
    hiddenItem->setData(0, VisibleRole, false);

    searchEdit->setText("Actor");

    QVERIFY(visibleItem->isHidden() == false);
    QVERIFY(hiddenItem->isHidden());
}

void TestFileTree::testErrorJsonSplitAcrossChunks()
{
    // divisi の標準出力が JSON の途中で分割されて届いても、エラーが失われないこと
    const QString json = R"({"Type":0,"Summary":0,"File":"C:/data/Actors.csv","Row":3,"Language":"en","Details":""})";
    const auto half = json.size() / 2;
    QVERIFY(QMetaObject::invokeMethod(tree.get(), "addErrorText", Q_ARG(QString, json.left(half))));
    QVERIFY(QMetaObject::invokeMethod(tree.get(), "addErrorText", Q_ARG(QString, json.mid(half))));

    const auto& errors = component->runtimeData->errors;
    QVERIFY(errors.contains("Actors"));
    QCOMPARE(errors.at("Actors").size(), size_t{1});
    QCOMPARE(errors.at("Actors")[0].row, std::int64_t{3});
}

QTEST_MAIN(TestFileTree)
#include "tst_FileTree.moc"
