#include "ScriptEditorSortFilterProxyModel.h"
#include "ScriptCSVTableModel.h"
#include "CSVEditorTableModel.h"

using namespace langscore;


void langscore::ScriptEditorSortFilterProxyModel::setHideUncheckRow(bool isHide)
{
    auto flagTest = this->_filterMode;
    if(isHide) {
        flagTest |= FilterMode::Script_HideUncheckRow;
    }
    else {
        flagTest &= ~FilterMode::Script_HideUncheckRow;
    }
    if(this->_filterMode != flagTest) {
        this->_filterMode = flagTest;
        this->invalidateFilter();
    }
}

bool ScriptEditorSortFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    auto* srcModel = dynamic_cast<ScriptCSVTableModel*>(sourceModel());
    if(srcModel == nullptr) {
        return true;
    }

    if(this->_filterMode.testFlag(FilterMode::Script_HideUncheckRow))
    {
        const QModelIndex idx = srcModel->index(sourceRow, 0, sourceParent);
        if(srcModel->data(idx, DataType::IgnoreScriptItem).toBool()) {
            return false;
        }
    }

    return CSVEditorSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}