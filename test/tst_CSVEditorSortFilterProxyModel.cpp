#include "test_common.h"
#include <QStandardItemModel>
#include "CSVEditorSortFilterProxyModel.h"

// 列0: 原文、列1: 翻訳（言語列 "en"）
class SourceModel : public QStandardItemModel
{
public:
    SourceModel() : QStandardItemModel(0, 2)
    {
        appendRow({new QStandardItem("apple"), new QStandardItem("Apfel")});
        appendRow({new QStandardItem("banana"), new QStandardItem("")});
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if(orientation == Qt::Horizontal && role == DataType::IsLanguageColumn) {
            return section == 1;
        }
        if(orientation == Qt::Horizontal && role == Qt::UserRole) {
            return section == 0 ? QString("original") : QString("en");
        }
        return QStandardItemModel::headerData(section, orientation, role);
    }
};

TEST(CSVEditorSortFilterProxyModel, HideTranslatedRow)
{
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterMode(FilterMode::HideTranslatedRow);
    ASSERT_EQ(proxy.rowCount(), 1);
    EXPECT_EQ(proxy.index(0, 0).data().toString(), QString("banana"));
}

// 不具合の再発防止
TEST(CSVEditorSortFilterProxyModel, FilterTextHidesNonMatchingRows)
{
    // 検索文字列に一致しない行は表示されないこと
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterText("ban");
    EXPECT_EQ(proxy.rowCount(), 1);
}

TEST(CSVEditorSortFilterProxyModel, FilterTextNoMatchHidesRows)
{
    // どの列にも一致しない検索文字列では、行が表示されないこと
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterText("zzz");
    EXPECT_EQ(proxy.rowCount(), 0);
}

TEST(CSVEditorSortFilterProxyModel, HideTranslatedRowWithFilterColumn)
{
    // 検索列を指定していても「翻訳済みの行を隠す」が適用されること
    SourceModel source;
    langscore::CSVEditorSortFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterTargetColumn(0);
    proxy.setFilterText("a");
    proxy.setFilterMode(FilterMode::HideTranslatedRow);
    ASSERT_EQ(proxy.rowCount(), 1);
    EXPECT_EQ(proxy.index(0, 0).data().toString(), QString("banana"));
}
