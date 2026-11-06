#pragma once
#include "CSVEditorSortFilterProxyModel.h"

namespace langscore 
{

class ScriptEditorSortFilterProxyModel : public CSVEditorSortFilterProxyModel
{
public:

    using CSVEditorSortFilterProxyModel::CSVEditorSortFilterProxyModel;

    void setHideUncheckRow(bool isHide);

protected:

    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
};

}