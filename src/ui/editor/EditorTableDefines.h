#pragma once
#include <qnamespace.h>
#include <QString>

enum class SortOrder { None, Ascending, Descending };

enum DataType {
    FilterText = Qt::UserRole,
    IgnoreScriptItem,
    IsLanguageColumn
};


enum Columns
{
    Include = 0,
    ScriptName,
    TextPoint,
    OriginalText,

    LangColumnStart
};

enum FilterMode : int
{
    NoFilter               = 0,
    HideTranslatedRow      = 1 << 0,
    HideWithFilterText     = 1 << 1,
    Script_HideUncheckRow  = 1 << 2
};
Q_DECLARE_FLAGS(FilterModes, FilterMode)


const static auto Column_ID_Include    = QString("include");
const static auto Column_ID_ScriptName = QString("scriptName");
const static auto Column_ID_TextPoint  = QString("textPoint");
const static auto Column_ID_Original   = QString("original");