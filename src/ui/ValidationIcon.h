#pragma once
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include "service/ValidationErrorInfo.h"

//検証結果を表すアイコン。表のセルとファイルツリーで同じものを使う
inline QIcon ValidationIcon(ValidationErrorInfo::ErrorType type)
{
    if(type == ValidationErrorInfo::Error) {
        return QApplication::style()->standardIcon(QStyle::SP_MessageBoxCritical);
    }
    if(type == ValidationErrorInfo::Warning) {
        return QApplication::style()->standardIcon(QStyle::SP_MessageBoxWarning);
    }
    return QIcon();
}
