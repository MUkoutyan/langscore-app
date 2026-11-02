#include <QTest>
#include <QStandardItemModel>
#include "CSVEditorSortFilterProxyModel.h"

// 列0: 原文、列1: 翻訳（言語列 "en"）
class SourceModel : public QStandardItemModel
{
public:
    SourceModel() : QStandardItemModel(0, 2) {
        appendRow({new QStandardItem("apple"), new QStandardItem("Apfel")});
        appendRow({new QStandardItem("banana"), new QStandardItem("")});
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override {
        if(orientation == Qt::Horizontal && role == DataType::IsLanguageColumn) {
            return section == 1;
        }
        if(orientation == Qt::Horizontal && role == Qt::UserRole) {
            return section == 0 ? QString("original") : QString("en");
        }
        return QStandardItemModel::headerData(section, orientation, role);
    }
};

class TestCSVEditorSortFilterProxyModel : public QObject
{
    Q_OBJECT

private slots:
    void testHideTranslatedRow();

    // 不具合の再発防止
    void testFilterTextHidesNonMatchingRows();
    void testFilterTextNoMatchHidesRows();
    void testHideTranslatedRowWithFilterColumn();
};

void TestCSVEditorSortFilterProxyModel::testFilterTextHidesNonMatchingRows()
{
    // 検索文字列に一致しない行は表示されないこと
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterText("ban");
    QCOMPARE(proxy.rowCount(), 1);
}

void TestCSVEditorSortFilterProxyModel::testHideTranslatedRow()
{
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterMode(FilterMode::HideTranslatedRow);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data().toString(), QString("banana"));
}

void TestCSVEditorSortFilterProxyModel::testFilterTextNoMatchHidesRows()
{
    // どの列にも一致しない検索文字列では、行が表示されないこと
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterText("zzz");
    QCOMPARE(proxy.rowCount(), 0);
}

void TestCSVEditorSortFilterProxyModel::testHideTranslatedRowWithFilterColumn()
{
    // 検索列を指定していても「翻訳済みの行を隠す」が適用されること
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterTargetColumn(0);
    proxy.setFilterText("a");
    proxy.setFilterMode(FilterMode::HideTranslatedRow);
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.index(0, 0).data().toString(), QString("banana"));
}

QTEST_MAIN(TestCSVEditorSortFilterProxyModel)
#include "tst_CSVEditorSortFilterProxyModel.moc"
