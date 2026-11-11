#include "test_common.h"
#include <QTemporaryDir>
#include <QFile>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QTextEdit>
#include <QSortFilterProxyModel>
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

        //MainCSVTable::showMainFileText と同じく original の後ろに言語列を入れるため、列は original, en, type の順になる
        model = std::make_unique<langscore::CSVEditorTableModel>();
        ASSERT_TRUE(model->loadFromJsonFile(jsonPath));
        model->insertColumn(1, "en");

        component = std::make_unique<ComponentBase>();
        manager = std::make_shared<EditDataManager>();
        editor = std::make_unique<CSVEditor>(component.get());
        editor->setModel(model.get());
    }

    void TearDown() override
    {
        //ビューがモデルを参照しているため、先にビューを破棄する
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

TEST_F(CSVEditorTest, PasteWindowsLineEnding)
{
    // 他のアプリの複数行の文章は1つのセルに入り、改行が \n に統一され、コピー時に付く末尾の改行は除かれること
    QGuiApplication::clipboard()->setText("Hi\r\nYo\r\n");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Hi\nYo"));
    EXPECT_EQ(model->getCell(1, 1).value(), QString());
}

TEST_F(CSVEditorTest, PastePlainTextWithComma)
{
    // 他のアプリの文章は、カンマや " を含んでも分割せずに1つのセルに入ること
    QGuiApplication::clipboard()->setText("Hello, \"world\"");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Hello, \"world\""));
    EXPECT_EQ(model->getCell(0, 2).value(), QString("name"));
}

TEST_F(CSVEditorTest, PasteSpreadsheetCells)
{
    // Excel などのタブ区切りの表は行・列に分かれ、" で始まる値は改行を含めて1つのセルに入ること。値の途中の " は残ること
    QGuiApplication::clipboard()->setText("\"Line1\nLine2\"\tX\r\nSay \"hi\", ok\tY\r\n");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Line1\nLine2"));
    EXPECT_EQ(model->getCell(1, 1).value(), QString("Say \"hi\", ok"));
    // 右隣の type 列は編集できないため書き換わらない
    EXPECT_EQ(model->getCell(0, 2).value(), QString("name"));
}

TEST_F(CSVEditorTest, CopyAndPasteInEditor)
{
    // エディター内でコピーした値は、カンマ・改行・空のセルを含めてそのまま貼り付けられること
    model->setCell(0, 1, "a, \"b\"\nc");
    model->setCell(1, 1, "");
    editor->selectionModel()->select(model->index(0, 1), QItemSelectionModel::ClearAndSelect);
    editor->selectionModel()->select(model->index(1, 1), QItemSelectionModel::Select);
    editor->copy();
    // 他のアプリにはタブ区切りの表で渡す
    EXPECT_EQ(QGuiApplication::clipboard()->text().replace("\r\n", "\n"), QString("\"a, \"\"b\"\"\nc\"\n\n"));

    model->setCell(0, 1, "x");
    model->setCell(1, 1, "y");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("a, \"b\"\nc"));
    EXPECT_EQ(model->getCell(1, 1).value(), QString());

    // 1つのセルをコピーした場合、他のアプリには値そのものを渡す
    SelectCell(0, 1);
    editor->copy();
    EXPECT_EQ(QGuiApplication::clipboard()->text().replace("\r\n", "\n"), QString("a, \"b\"\nc"));
}

TEST_F(CSVEditorTest, PasteSkipsReadOnlyColumn)
{
    // 言語列から右へはみ出して貼り付けても、編集できない type 列は書き換わらないこと
    QGuiApplication::clipboard()->setText("Hi\tChanged");
    SelectCell(0, 1);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Hi"));
    EXPECT_EQ(model->getCell(0, 2).value(), QString("name"));
}

TEST_F(CSVEditorTest, ClearSkipsOriginalColumn)
{
    // original 列と言語列をまとめて選択して削除しても、original 列は消えないこと
    //CSVEditor::setText は this をキャプチャした singleShot を登録するため、テストではモデルへ直接書き込む
    model->setCell(0, 1, "Hi");
    editor->selectionModel()->select(model->index(0, 0), QItemSelectionModel::ClearAndSelect);
    editor->selectionModel()->select(model->index(0, 1), QItemSelectionModel::Select);
    editor->clearSelectedCells();
    EXPECT_EQ(model->getCell(0, 0).value(), QString("A"));
    EXPECT_EQ(model->getCell(0, 1).value(), QString());
}

TEST_F(CSVEditorTest, PasteSingleValueFillsSelection)
{
    // 1つの値を複数のセルに貼り付けると、選択したすべてのセルに入ること
    QGuiApplication::clipboard()->setText("Same");
    editor->selectionModel()->select(model->index(0, 1), QItemSelectionModel::ClearAndSelect);
    editor->selectionModel()->select(model->index(1, 1), QItemSelectionModel::Select);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Same"));
    EXPECT_EQ(model->getCell(1, 1).value(), QString("Same"));
}

TEST_F(CSVEditorTest, NextCellStopsAtTableEdge)
{
    // 次のセルへの移動は編集できない列を飛ばし、表の端では動かないこと
    editor->selectionModel()->setCurrentIndex(model->index(0, 1), QItemSelectionModel::ClearAndSelect);
    editor->selectAndEditNextCell(editor->currentIndex(), 0, 1);
    EXPECT_EQ(editor->currentIndex(), model->index(0, 1));
    editor->selectAndEditNextCell(editor->currentIndex(), 1, 0);
    EXPECT_EQ(editor->currentIndex(), model->index(1, 1));
    editor->selectAndEditNextCell(editor->currentIndex(), 1, 0);
    EXPECT_EQ(editor->currentIndex(), model->index(1, 1));
}

TEST_F(CSVEditorTest, NextCellFromEditedCell)
{
    // 飛び飛びに選択した場合など、次のセルは currentIndex ではなく編集したセルから決めること
    editor->selectionModel()->setCurrentIndex(model->index(1, 1), QItemSelectionModel::ClearAndSelect);
    editor->selectAndEditNextCell(model->index(0, 0), 0, 1);
    EXPECT_EQ(editor->currentIndex(), model->index(0, 1));
}

TEST_F(CSVEditorTest, UndoAfterSwitchingModel)
{
    // 別のファイルを開いて proxy の元のモデルを差し替えた後でも、編集を元に戻せること
    QSortFilterProxyModel proxy;
    proxy.setSourceModel(model.get());
    editor->setModel(&proxy);
    QGuiApplication::clipboard()->setText("Hi");
    editor->selectionModel()->select(proxy.index(0, 1), QItemSelectionModel::ClearAndSelect);
    editor->paste();
    EXPECT_EQ(model->getCell(0, 1).value(), QString("Hi"));

    proxy.setSourceModel(nullptr);
    editor->undo();
    EXPECT_EQ(model->getCell(0, 1).value(), QString());
    editor->setModel(model.get());
}

TEST_F(CSVEditorTest, TypingReplacesText)
{
    // セルを選択して文字を入力すると、元の文章を置き換えて編集を始めること
    model->setCell(0, 1, "Old");
    editor->selectionModel()->setCurrentIndex(model->index(0, 1), QItemSelectionModel::ClearAndSelect);
    QKeyEvent key(QEvent::KeyPress, Qt::Key_X, Qt::NoModifier, "x");
    QCoreApplication::sendEvent(editor.get(), &key);
    auto* textEdit = editor->findChild<QTextEdit*>();
    ASSERT_NE(textEdit, nullptr);
    EXPECT_EQ(textEdit->toPlainText(), QString("x"));
}

TEST_F(CSVEditorTest, ModifiedClearedWhenReverted)
{
    // 保存時の値に戻すと、変更あり状態が解除されること
    EXPECT_FALSE(model->isModified());
    model->setData(model->index(0, 1), "Hi");
    EXPECT_TRUE(model->isModified());
    model->setData(model->index(0, 1), "");
    EXPECT_FALSE(model->isModified());
}
