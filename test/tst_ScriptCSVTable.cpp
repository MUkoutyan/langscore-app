#include "test_common.h"
#include <QPointer>
#include "ScriptCSVTable.h"
#include "ScriptCSVTableModel.h"
#include "EditDataManager.h"

// 不具合の再発防止
TEST(ScriptCSVTable, ClearKeepsManagerOwnedModel)
{
    // clear() が EditDataManager の所有するモデルを削除しないこと
    ComponentBase component;
    auto manager = std::make_shared<EditDataManager>();
    auto table = std::make_unique<ScriptCSVTable>(&component, manager);

    // ScriptCSVTable::setupScriptTable と同じパスでモデルを取得する
    const auto modelPath = QString("/editing/Scripts.lsjson");
    table->setupScriptTable();
    QPointer<ScriptCSVTableModel> model = manager->getScriptCSVModel(modelPath);
    ASSERT_NE(model, nullptr);

    table->clear();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    const bool alive = (model.isNull() == false);
    if(alive == false) {
        // 削除済みのモデルを manager が再度 delete して二重解放になるため、意図的に破棄しない
        new std::shared_ptr<EditDataManager>(manager);
    }
    EXPECT_TRUE(alive);
}

TEST(ScriptCSVTable, SelectRowOfScriptExactName)
{
    // ファイルツリーでスクリプトを選ぶと、名前がそのスクリプトと一致する最初の行が選ばれること。
    // 名前の一部が一致する別のスクリプト（"A" に対する "AB"）の行は選ばない
    ComponentBase component;
    auto manager = std::make_shared<EditDataManager>();
    auto table = std::make_unique<ScriptCSVTable>(&component, manager);
    struct Access : ComponentBase { using ComponentBase::setting; };
    auto& setting = *static_cast<Access&>(component).setting;
    setting.projectType = settings::MZ;
    for(const auto& fileName : {QString("AB.js"), QString("A.js")})
    {
        settings::ScriptInfo info;
        info.fileName = fileName;
        info.scriptName = fileName;
        langscore::ScriptTextPosition line;
        line.originalText = "Hello " + fileName;
        line.d = langscore::ScriptTextPosition::RowCol{1, 2};
        info.lines.push_back(line);
        setting.writeObj.scriptInfo.push_back(info);
    }
    table->setupScriptTable();

    table->onScriptTableSelectRow("A");
    auto* editor = table->findChild<CSVEditor*>();
    ASSERT_NE(editor, nullptr);
    const auto selected = editor->selectionModel()->selectedRows(Columns::OriginalText);
    ASSERT_EQ(selected.size(), 1);
    EXPECT_EQ(selected[0].data().toString(), QString("Hello A.js"));
}
