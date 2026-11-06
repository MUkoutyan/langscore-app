#include "invoker.h"
#include <QApplication>
#include <QFile>
#include <QStringDecoder>
#include <QDialog>
#include <QVBoxLayout>

invoker::invoker(ComponentBase *setting)
    : ComponentBase(setting){
}

void invoker::analyze(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--analyze"});
}

void invoker::reanalysis(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--reanalysis"});
}

void invoker::exportFirstTime(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--exportCSV", "--updatePlugin"});
}

void invoker::exportCSV(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--exportCSV"});
}

void invoker::updatePlugin(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--updatePlugin"});
}

void invoker::validate(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--validate"});
}

void invoker::packing(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--packing"});
}

void invoker::doProcess(QStringList option)
{
    this->passOption = option;
    auto process = new QProcess();
    //読み込み単位でマルチバイト文字が分断されても化けないよう、ストリームごとに状態を持つデコーダーを使う
    auto stdOutDecoder = std::make_shared<QStringDecoder>(QStringDecoder::Utf8);
    auto stdErrDecoder = std::make_shared<QStringDecoder>(QStringDecoder::Utf8);
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process, stdOutDecoder](){
        QString message = stdOutDecoder->decode(process->readAllStandardOutput());
        emit this->getStdOut(message);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process, stdErrDecoder](){
        QString message = stdErrDecoder->decode(process->readAllStandardError());
        emit this->getStdOut(message);
    });
    connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [process, this](int exitCode, QProcess::ExitStatus status){
        emit this->finish(exitCode, status);
        process->deleteLater();
    });

    auto divisiPath = qApp->applicationDirPath()+"/bin/divisi.exe";
    emit this->getStdOut("run : " + divisiPath + " " + option.join(" "));
    process->start(divisiPath, option);
    if (!process->waitForStarted(-1)) {
        emit this->getStdOut(process->errorString());
        delete process;
        //起動に失敗した場合も finish を通知し、呼び出し側が UI の状態を戻せるようにする。
        //非同期の場合は呼び出し元の処理が終わってから通知する。
        if(sync) {
            emit this->finish(-1, QProcess::CrashExit);
        }
        else {
            QMetaObject::invokeMethod(this, [this]() { emit this->finish(-1, QProcess::CrashExit); }, Qt::QueuedConnection);
        }
        return;
    }

    if(sync){
        process->waitForFinished(-1);
    }
}
