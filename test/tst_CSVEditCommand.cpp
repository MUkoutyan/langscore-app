#include "test_common.h"
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include "CSVEditCommand.h"

// CSVEditor と同様に、プロキシモデル経由で setData する
struct ProxyListener : public CSVEditCommand::CellEditsListener
{
    QStandardItemModel source{2, 1};
    QSortFilterProxyModel proxy;
    ProxyListener() { proxy.setSourceModel(&source); }

    void setData(QModelIndex index, QVariant value, int role) override {
        proxy.setData(index, value, role);
    }
};

// setData を QStandardItemModel に転送し、呼ばれた回数を記録する
struct ModelListener : public CSVEditCommand::CellEditsListener
{
    QStandardItemModel model{2, 2};
    int setCount = 0;

    void setData(QModelIndex index, QVariant value, int role) override
    {
        setCount++;
        model.setData(index, value, role);
    }
};

TEST(CSVEditCommand, RedoUndo)
{
    ModelListener listener;
    auto a = listener.model.index(0, 0);
    auto b = listener.model.index(1, 1);
    listener.model.setData(a, "oldA");
    listener.model.setData(b, "oldB");

    QUndoStack stack;
    stack.push(new CSVEditCommand(&listener, {{a, "oldA", "newA"}, {b, "oldB", "newB"}}));
    EXPECT_EQ(a.data().toString(), QString("newA"));
    EXPECT_EQ(b.data().toString(), QString("newB"));

    stack.undo();
    EXPECT_EQ(a.data().toString(), QString("oldA"));
    EXPECT_EQ(b.data().toString(), QString("oldB"));

    stack.redo();
    EXPECT_EQ(a.data().toString(), QString("newA"));
    EXPECT_EQ(b.data().toString(), QString("newB"));
}

TEST(CSVEditCommand, InvalidIndexSkipped)
{
    ModelListener listener;
    CSVEditCommand cmd(&listener, {{QModelIndex(), "", "x"}});
    cmd.redo();
    EXPECT_EQ(listener.setCount, 0);
}

TEST(CSVEditCommand, Text)
{
    ModelListener listener;
    EXPECT_EQ(CSVEditCommand(&listener, {}).text(), QString("Edit Cells"));
    EXPECT_EQ(CSVEditCommand(&listener, {}, "Paste").text(), QString("Paste"));
}

// 不具合の再発防止
TEST(CSVEditCommand, UndoAfterSort)
{
    // 編集後に並び替えてから Undo しても、編集したセルが元に戻ること
    ProxyListener listener;
    listener.source.setData(listener.source.index(0, 0), "b");
    listener.source.setData(listener.source.index(1, 0), "a");

    QUndoStack stack;
    auto edited = listener.proxy.index(0, 0);   // "b" のセル
    stack.push(new CSVEditCommand(&listener, {{edited, "b", "x"}}));
    EXPECT_EQ(listener.source.index(0, 0).data().toString(), QString("x"));

    listener.proxy.sort(0);                     // 表示順: "a", "x"
    stack.undo();

    EXPECT_EQ(listener.source.index(0, 0).data().toString(), QString("b"));
    EXPECT_EQ(listener.source.index(1, 0).data().toString(), QString("a"));
}
