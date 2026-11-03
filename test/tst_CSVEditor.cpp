#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include "CSVEditor.h"
#include "CSVEditorTableModel.h"
#include "EditDataManager.h"

class CSVEditorTest : public ::testing::Test
{
protected:
    QTemporaryDir tempDir;
    std::unique_ptr<ComponentBase> component;
    std::shared_ptr<EditDataManager> manager;
    std::unique_ptr<langscore::CSVEditorTableModel> model;
    std::unique_ptr<CSVEditor> editor;

    void SetUp() override
    {
        const auto jsonPath = tempDir.filePath("Actors.json");
        QFile file(jsonPath);
        ASSERT_TRUE(file.open(QFile::WriteOnly));
        file.write(R"([
            {"original": "A", "type": ["name"]},
            {"original": "B", "type": ["name"]}
        ])");
        file.close();

        //列は original, en, type の順になる（MainCSVTable::showMainFileText と同じく original の後ろに言語列を入れる）
        model = std::make_unique<langscore::CSVEditorTableModel>();
        ASSERT_TRUE(model->loadFromJsonFile(jsonPath));
        model->insertColumn(1, "en");

        component = std::make_unique<ComponentBase>();
        manager = std::make_shared<EditDataManager>();
        editor = std::make_unique<CSVEditor>(manager, component.get());
        editor->setModel(model.get());
    }

    void TearDown() override
    {
        //ビューがモデルを参照しているので、先にビューを破棄する
        editor.reset();
        model.reset();
        manager.reset();
        component.reset();
    }

    void SelectCell(int row, int column)
    {
        editor->selectionModel()->select(model->index(row, column), QItemSelectionModel::ClearAndSelect);
    }
};

// RED: Docs/bugfix-proposals.md の不具合。修正後に通る
TEST_F(CSVEditorTest, PasteWindowsLineEnding) // [3]
{
    // Windows のクリップボード（改行が \r\n）から貼り付けても、セルの末尾に \r が残らないこと
    QGuiApplication::clipboard()->setText("Hi\r\nYo\r\n");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Hi"));
    EXPECT_EQ(model->getCell(1, 1).value(), QString("Yo"));
}

TEST_F(CSVEditorTest, PasteSkipsReadOnlyColumn) // [1]
{
    // 言語列から右へはみ出して貼り付けても、編集できない type 列は書き換わらないこと
    QGuiApplication::clipboard()->setText("Hi,Changed");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Hi"));
    EXPECT_EQ(model->getCell(0, 2).value(), QString("name"));
}

TEST_F(CSVEditorTest, ClearSkipsOriginalColumn) // [1]
{
    // original 列と言語列をまとめて選んで削除しても、original 列は消えないこと
    //CSVEditor::setText は this を捕まえた singleShot を登録するため、テストではモデルへ直接書き込む
    model->setCell(0, 1, "Hi");
    editor->selectionModel()->select(model->index(0, 0), QItemSelectionModel::ClearAndSelect);
    editor->selectionModel()->select(model->index(0, 1), QItemSelectionModel::Select);
    editor->clearSelectedCells();
    EXPECT_EQ(model->getCell(0, 0).value(), QString("A"));
    EXPECT_EQ(model->getCell(0, 1).value(), QString());
}
