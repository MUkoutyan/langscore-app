#include <QTest>
#include <QPointer>
#include "ScriptCSVTable.h"
#include "ScriptCSVTableModel.h"
#include "EditDataManager.h"

class TestScriptCSVTable : public QObject
{
    Q_OBJECT

private slots:
    // 不具合の再発防止
    void testClearKeepsManagerOwnedModel();
};

void TestScriptCSVTable::testClearKeepsManagerOwnedModel()
{
    // clear() が EditDataManager の所有するモデルを削除しないこと
    ComponentBase component;
    auto manager = std::make_shared<EditDataManager>();
    auto table = std::make_unique<ScriptCSVTable>(&component, manager);

    // ScriptCSVTable::setupScriptTable と同じパスでモデルを取得する
    const auto modelPath = QString("/editing/Scripts.lsjson");
    table->setupScriptTable();
    QPointer<ScriptCSVTableModel> model = manager->getScriptCSVModel(modelPath);
    QVERIFY(model != nullptr);

    table->clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const bool alive = (model.isNull() == false);
    if(alive == false) {
        // 削除済みのモデルを manager が再度 delete して二重解放になるため、意図的に破棄しない
        new std::shared_ptr<EditDataManager>(manager);
    }
    QVERIFY(alive);
}

QTEST_MAIN(TestScriptCSVTable)
#include "tst_ScriptCSVTable.moc"
