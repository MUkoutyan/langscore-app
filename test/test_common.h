#pragma once

#include <gtest/gtest.h>
#include <QString>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <functional>
#include <ostream>

// 比較に失敗したとき、QString の中身を文字列として表示する（QStringList などの要素の表示にも使われる）
inline void PrintTo(const QString& value, std::ostream* os)
{
    *os << '"' << value.toStdString() << '"';
}

// 条件が満たされるまでイベントループを回して待つ。時間内に満たされなければ false を返す
inline bool WaitUntil(const std::function<bool()>& condition, int timeoutMs)
{
    QDeadlineTimer deadline(timeoutMs);
    while(condition() == false)
    {
        if(deadline.hasExpired()) { return false; }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
}
