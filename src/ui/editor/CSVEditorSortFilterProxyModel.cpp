#include "CSVEditorSortFilterProxyModel.h"
#include "CSVEditorTableModel.h"
#include <QAbstractItemModel>
#include <QSettings>
#include <QApplication>

namespace langscore {

CSVEditorSortFilterProxyModel::CSVEditorSortFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent)
{
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void CSVEditorSortFilterProxyModel::setFilterText(const QString& text)
{
    if(_filterText == text) {
        return;
    }
    _filterText = text;
    invalidateFilter();
}

void CSVEditorSortFilterProxyModel::setFilterTargetColumn(int column)
{
    if(_filterColumn == column) {
        return;
    }
    _filterColumn = column;
    invalidateFilter();
}

void CSVEditorSortFilterProxyModel::setFilterMode(FilterModes mode)
{
    if(this->_filterMode == mode) { return; }

    this->_filterMode = mode;
    invalidateFilter();
}

void CSVEditorSortFilterProxyModel::setFilterNoTranslateColumn(bool isFilter)
{
    auto flagTest = this->_filterMode;
    if(isFilter) {
        flagTest |= FilterMode::HideTranslatedRow;
    }
    else {
        flagTest &= ~FilterMode::HideTranslatedRow;
    }
    if(this->_filterMode != flagTest) {
        this->_filterMode = flagTest;
        this->invalidateFilter();
    }
}

void CSVEditorSortFilterProxyModel::setLanguageColumnHidden(const QString& langCode, bool hidden)
{
    const QString key = langCode.toLower();
    if(hidden) {
        _hiddenLanguages.insert(key);
    } else {
        _hiddenLanguages.remove(key);
    }
    invalidateFilter();
    emit columnVisibilityChanged();
}

bool CSVEditorSortFilterProxyModel::isLanguageColumnHidden(const QString& langCode) const
{
    return _hiddenLanguages.contains(langCode.toLower());
}

void CSVEditorSortFilterProxyModel::clearAllHiddenLanguages()
{
    if(_hiddenLanguages.isEmpty()) {
        return;
    }
    _hiddenLanguages.clear();
    invalidateFilter();
    emit columnVisibilityChanged();
}

void CSVEditorSortFilterProxyModel::loadColumnFilterFromSettings()
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.beginGroup("filters");
    const QStringList keys = settings.childKeys();
    for(const QString& key : keys) {
        const bool visible = settings.value(key, true).toBool();
        if(!visible) {
            _hiddenLanguages.insert(key.toLower());
        } else {
            _hiddenLanguages.remove(key.toLower());
        }
    }
    settings.endGroup();
    invalidateFilter();
    emit columnVisibilityChanged();
}

void CSVEditorSortFilterProxyModel::saveColumnFilterToSettings(
    const std::vector<std::pair<QString, bool>>& languageVisibilities)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.beginGroup("filters");
    for(const auto& [lang, visible] : languageVisibilities) {
        settings.setValue(lang.toLower(), visible);
    }
    settings.endGroup();
    settings.sync();
}

void CSVEditorSortFilterProxyModel::cycleColumnSort(int proxyColumn)
{
    SortOrder newOrder;
    if(_sortColumn != proxyColumn) {
        newOrder = SortOrder::Ascending;
    } 
    else 
    {
        if(_sortOrder == SortOrder::None) { 
            newOrder = SortOrder::Ascending; 
        } else if(_sortOrder == SortOrder::Ascending) { 
            newOrder = SortOrder::Descending; 
        } else { 
            newOrder = SortOrder::None; 
        }
    }
    setSortState(proxyColumn, newOrder);
}

void CSVEditorSortFilterProxyModel::setSortState(int proxyColumn, SortOrder order)
{
    _sortOrder = order;
    _sortColumn = (order == SortOrder::None) ? -1 : proxyColumn;

    if(order == SortOrder::None) {
        sort(-1, Qt::AscendingOrder);
    } else if(order == SortOrder::Ascending) {
        sort(proxyColumn, Qt::AscendingOrder);
    } else {
        sort(proxyColumn, Qt::DescendingOrder);
    }

    emit sortStateChanged(_sortColumn, _sortOrder);
}

bool CSVEditorSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    QAbstractItemModel* srcModel = sourceModel();
    if(srcModel == nullptr) { return true; }

    if(_filterText.isEmpty() == false) 
    {
        const int colCount = srcModel->columnCount(sourceParent);
        // 特定列のみ検索
        if(_filterColumn >= 0 && _filterColumn < colCount) {
            const QModelIndex idx = srcModel->index(sourceRow, _filterColumn, sourceParent);
            return srcModel->data(idx, Qt::DisplayRole).toString().contains(_filterText, Qt::CaseInsensitive);
        }

        // 全列を検索
        for(int col = 0; col < colCount; ++col) {
            const QModelIndex idx = srcModel->index(sourceRow, col, sourceParent);
            if(srcModel->data(idx, Qt::DisplayRole).toString().contains(_filterText, Qt::CaseInsensitive)) {
                return true;
            }
        }
    }

    if(this->_filterMode.testFlag(FilterMode::HideTranslatedRow))
    {
        const int colCount = srcModel->columnCount(sourceParent);
        for(int col = 0; col < colCount; ++col) 
        {
            const auto isLangHeader = srcModel->headerData(col, Qt::Horizontal, DataType::IsLanguageColumn).toBool();
            if(isLangHeader == false) { continue; }

            const QModelIndex idx = srcModel->index(sourceRow, col, sourceParent);
            if(srcModel->data(idx, Qt::DisplayRole).toString().isEmpty() == false) {
                return false;
            }
        }
    }

    return true;
}

bool CSVEditorSortFilterProxyModel::filterAcceptsColumn(int sourceColumn, const QModelIndex&) const
{
    if(_hiddenLanguages.isEmpty()) {
        return true;
    }

    QAbstractItemModel* srcModel = sourceModel();
    if(srcModel == nullptr) {
        return true;
    }

    const auto isLangHeader = srcModel->headerData(sourceColumn, Qt::Horizontal, DataType::IsLanguageColumn).toBool();
    if(isLangHeader == false) {
        return true;
    }

    const QString header = srcModel->headerData(sourceColumn, Qt::Horizontal, Qt::UserRole).toString().toLower();
    return _hiddenLanguages.contains(header) == false;
}

bool CSVEditorSortFilterProxyModel::lessThan(const QModelIndex& left, const QModelIndex& right) const
{
    const QString lhs = sourceModel()->data(left,  Qt::DisplayRole).toString();
    const QString rhs = sourceModel()->data(right, Qt::DisplayRole).toString();
    return lhs.compare(rhs, Qt::CaseInsensitive) < 0;
}

} // namespace langscore