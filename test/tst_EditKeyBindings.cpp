#include "test_common.h"
#include <QKeyEvent>
#include <QSettings>
#include <QLabel>
#include <QStandardItemModel>
#include "MultiLineEditDelegate.h"
#include "dialog/EditKeySettingsDialog.h"

using TextEdit = MultiLineEditDelegate::MultiLineTextEdit;

namespace
{
    //キーを押し、編集の終わり方を返す（終わらなければ -1）
    int Press(TextEdit& edit, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier)
    {
        int moveTo = -1;
        auto connection = QObject::connect(&edit, &TextEdit::editingFinished, [&moveTo](TextEdit::MoveTo m) { moveTo = static_cast<int>(m); });
        QKeyEvent event(QEvent::KeyPress, key, modifiers);
        QCoreApplication::sendEvent(&edit, &event);
        QObject::disconnect(connection);
        return moveTo;
    }
}

TEST(EditKeyBindingsTest, DefaultKeys)
{
    // 既定では Enter で改行し、Ctrl+Enter で確定、Tab で確定して右へ移ること
    TextEdit edit;
    EXPECT_EQ(Press(edit, Qt::Key_Return), -1);
    EXPECT_EQ(edit.toPlainText(), QString("\n"));
    EXPECT_EQ(Press(edit, Qt::Key_Tab), static_cast<int>(TextEdit::MoveTo::Right));

    TextEdit edit2;
    EXPECT_EQ(Press(edit2, Qt::Key_Enter, Qt::ControlModifier | Qt::KeypadModifier), static_cast<int>(TextEdit::MoveTo::None));
}

TEST(EditKeyBindingsTest, CustomKeys)
{
    // Enter で確定、Shift+Enter で改行にすると、割り当てたとおりに動き、割り当てていない Ctrl+Enter では何もしないこと
    auto keys = EditKeyBindings::defaults();
    keys.keys[EditKeyBindings::Confirm] = QKeySequence(Qt::Key_Return);
    keys.keys[EditKeyBindings::NewLine] = QKeySequence(Qt::SHIFT | Qt::Key_Return);
    keys.keys[EditKeyBindings::ConfirmAndMoveDown] = QKeySequence();
    EXPECT_TRUE(EditKeySettingsDialog::duplicatedActions(keys).isEmpty());

    TextEdit edit;
    edit.setKeyBindings(keys);
    EXPECT_EQ(Press(edit, Qt::Key_Return, Qt::ShiftModifier), -1);
    EXPECT_EQ(Press(edit, Qt::Key_Return, Qt::ControlModifier), -1);
    EXPECT_EQ(edit.toPlainText(), QString("\n"));
    EXPECT_EQ(Press(edit, Qt::Key_Return), static_cast<int>(TextEdit::MoveTo::None));

    //同じキーを2つの動作に割り当てると分かること
    keys.keys[EditKeyBindings::Cancel] = QKeySequence(Qt::Key_Return);
    EXPECT_EQ(EditKeySettingsDialog::duplicatedActions(keys).size(), 2);

    //テンキーの Enter と Enter は、同じキーとして扱うこと
    keys.keys[EditKeyBindings::Cancel] = QKeySequence(QKeyCombination(Qt::KeypadModifier, Qt::Key_Enter));
    EXPECT_EQ(EditKeySettingsDialog::duplicatedActions(keys).size(), 2);
}

TEST(EditKeyBindingsTest, SaveAndLoad)
{
    // 保存したキーを次に読み込めること
    auto keys = EditKeyBindings::defaults();
    keys.keys[EditKeyBindings::Confirm] = QKeySequence(Qt::ALT | Qt::Key_Return);
    keys.save();
    EXPECT_EQ(EditKeyBindings::load().keys[EditKeyBindings::Confirm], QKeySequence(Qt::ALT | Qt::Key_Return));

    //他のテストに残さない
    QSettings(QCoreApplication::applicationDirPath() + "/settings.ini", QSettings::IniFormat).remove("EditKeys");
    EXPECT_EQ(EditKeyBindings::load().keys[EditKeyBindings::Confirm], QKeySequence(Qt::CTRL | Qt::Key_Return));
}

TEST(EditKeyBindingsTest, GuideWrapsBetweenItems)
{
    // セルの幅が狭いと、キーの説明は説明と説明の間で改行され、説明の途中では改行されないこと
    MultiLineEditDelegate delegate;
    QWidget parent;
    QStyleOptionViewItem option;
    auto* editor = delegate.createEditor(&parent, option, QModelIndex());
    auto* label = editor->findChild<QLabel*>();
    ASSERT_NE(label, nullptr);
    const auto items = EditKeyBindings::defaults().guideItems();

    QStandardItemModel model(1, 1);
    option.rect = QRect(0, 0, 120, 30);
    delegate.updateEditorGeometry(editor, option, model.index(0, 0));
    const auto lines = label->text().split('\n');
    EXPECT_EQ(lines.size(), items.size());
    EXPECT_TRUE(lines.contains(items[0]));

    option.rect = QRect(0, 0, 3000, 30);
    delegate.updateEditorGeometry(editor, option, model.index(0, 0));
    EXPECT_EQ(label->text().split('\n').size(), 1);
    delete editor;
}
