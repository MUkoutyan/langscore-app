#pragma once

#include <gtest/gtest.h>
#include <QString>
#include <QCoreApplication>
#include <QDeadlineTimer>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <functional>
#include <ostream>

// 比較に失敗したとき、QString の中身を文字列として表示する。QStringList などの要素の表示にも使われる
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

// フォルダの中身を空のフォルダも含めてすべてコピーする。コピー先に同じファイルがあれば置き換える。
// 元が読み取り専用でも、コピーしたファイルは書き込めるようにする。コピーしたファイルのパスを返す
inline QStringList CopyDirectory(const QString& source, const QString& destination)
{
    QStringList copied;
    QDirIterator it(source, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while(it.hasNext())
    {
        const auto filePath = it.next();
        const auto target = destination + "/" + QDir(source).relativeFilePath(filePath);
        if(it.fileInfo().isDir()) {
            QDir().mkpath(target);
            continue;
        }
        QDir().mkpath(QFileInfo(target).absolutePath());
        QFile::remove(target);
        if(QFile::copy(filePath, target)) {
            QFile::setPermissions(target, QFile::permissions(target) | QFileDevice::WriteOwner | QFileDevice::WriteUser);
            copied.append(target);
        }
    }
    return copied;
}

struct ProcessResult
{
    int exitCode = -1;
    QString output;
};

// プロセスを実行して終わるまで待つ。時間内に終わらなければ停止して output を "timeout" にする
inline ProcessResult RunProcess(const QString& program, const QStringList& arguments, int timeoutMs = 120000)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(program, arguments);
    ProcessResult result;
    if(process.waitForFinished(timeoutMs) == false) {
        process.kill();
        process.waitForFinished();
        result.output = "timeout: " + process.errorString();
        return result;
    }
    result.exitCode = process.exitCode();
    result.output = QString::fromUtf8(process.readAll());
    return result;
}

#ifdef LANGSCORE_DIVISI_PATH
// アプリの invoker と同じ形式の引数で divisi を実行する
inline ProcessResult RunDivisi(const QString& configPath, const QString& option)
{
    return RunProcess(LANGSCORE_DIVISI_PATH, {"-c", "\"" + configPath + "\"", option});
}
#endif
