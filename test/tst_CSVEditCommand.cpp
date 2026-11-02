#include <QTest>
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

// setData を QStandardItemModel に転送し、呼び出し時の抑制フラグを記録する
struct ModelListener : public CSVEditCommand::CellEditsListener
{
    QStandardItemModel model{2, 2};
    bool suppressedDuringSet = false;

    void setData(QModelIndex index, QVariant value, int role) override {
        suppressedDuringSet = _suppressUndoTracking;
        model.setData(index, value, role);
    }
};

class TestCSVEditCommand : public QObject
{
    Q_OBJECT

private slots:
    void testRedoUndo();
    void testFlags();
    void testInvalidIndexSkipped();
    void testText();

    // 不具合の再発防止
    void testUndoAfterSort();
};

void TestCSVEditCommand::testRedoUndo()
{
    ModelListener listener;
    auto a = listener.model.index(0, 0);
    auto b = listener.model.index(1, 1);
    listener.model.setData(a, "oldA");
    listener.model.setData(b, "oldB");

    QUndoStack stack;
    stack.push(new CSVEditCommand(&listener, {{a, "oldA", "newA"}, {b, "oldB", "newB"}}));
    QCOMPARE(a.data().toString(), QString("newA"));
    QCOMPARE(b.data().toString(), QString("newB"));

    stack.undo();
    QCOMPARE(a.data().toString(), QString("oldA"));
    QCOMPARE(b.data().toString(), QString("oldB"));

    stack.redo();
    QCOMPARE(a.data().toString(), QString("newA"));
    QCOMPARE(b.data().toString(), QString("newB"));
}

void TestCSVEditCommand::testFlags()
{
    ModelListener listener;
    auto a = listener.model.index(0, 0);
    CSVEditCommand cmd(&listener, {{a, "", "x"}});
    cmd.redo();
    QVERIFY(listener.suppressedDuringSet);
    QVERIFY(listener._suppressUndoTracking == false);
    QVERIFY(listener._isModified);
}

void TestCSVEditCommand::testInvalidIndexSkipped()
{
    ModelListener listener;
    CSVEditCommand cmd(&listener, {{QModelIndex(), "", "x"}});
    cmd.redo();
    QVERIFY(listener.suppressedDuringSet == false);
}

void TestCSVEditCommand::testText()
{
    ModelListener listener;
    QCOMPARE(CSVEditCommand(&listener, {}).text(), QString("Edit Cells"));
    QCOMPARE(CSVEditCommand(&listener, {}, "Paste").text(), QString("Paste"));
}

void TestCSVEditCommand::testUndoAfterSort()
{
    // 編集後に並び替えてから Undo しても、編集したセルが元に戻ること
    ProxyListener listener;
    listener.source.setData(listener.source.index(0, 0), "b");
    listener.source.setData(listener.source.index(1, 0), "a");

    QUndoStack stack;
    auto edited = listener.proxy.index(0, 0);   // "b" のセル
    stack.push(new CSVEditCommand(&listener, {{edited, "b", "x"}}));
    QCOMPARE(listener.source.index(0, 0).data().toString(), QString("x"));

    listener.proxy.sort(0);                     // 表示順: "a", "x"
    stack.undo();

    QCOMPARE(listener.source.index(0, 0).data().toString(), QString("b"));
    QCOMPARE(listener.source.index(1, 0).data().toString(), QString("a"));
}

QTEST_MAIN(TestCSVEditCommand)
#include "tst_CSVEditCommand.moc"
