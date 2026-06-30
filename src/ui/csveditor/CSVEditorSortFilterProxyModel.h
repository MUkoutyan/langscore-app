#pragma once

#include <QSortFilterProxyModel>
#include <QSet>
#include <QString>
#include <vector>
#include <utility>

namespace langscore {

class CSVEditorSortFilterProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum class SortOrder { None, Ascending, Descending };

    explicit CSVEditorSortFilterProxyModel(QObject* parent = nullptr);

    // テキストフィルタ（空文字列でフィルタ解除）
    void setFilterText(const QString& text);

    // フィルタ対象列（-1 で全列を検索）
    void setFilterTargetColumn(int column);

    QString filterText() const { return _filterText; }
    int filterTargetColumn() const { return _filterColumn; }

    // 言語列の表示/非表示
    void setLanguageColumnHidden(const QString& langCode, bool hidden);
    bool isLanguageColumnHidden(const QString& langCode) const;
    void clearAllHiddenLanguages();
    void loadColumnFilterFromSettings();
    void saveColumnFilterToSettings(const std::vector<std::pair<QString, bool>>& languageVisibilities);

    // 3ステートソート（なし → 昇順 → 降順 → なし）
    void cycleColumnSort(int proxyColumn);
    void setSortState(int proxyColumn, SortOrder order);
    int sortedColumn() const { return _sortColumn; }
    SortOrder currentSortOrder() const { return _sortOrder; }

signals:
    void sortStateChanged(int column, SortOrder order);
    void columnVisibilityChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool filterAcceptsColumn(int sourceColumn, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString _filterText;
    int _filterColumn = -1;
    QSet<QString> _hiddenLanguages;
    int _sortColumn = -1;
    SortOrder _sortOrder = SortOrder::None;
};

} // namespace langscore
