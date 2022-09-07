#include "invoker.h"
#include <QApplication>
#include <QFile>
#include <QDialog>
#include <QVBoxLayout>

invoker::invoker(ComponentBase *setting)
    : ComponentBase(setting){
}

void invoker::analyze(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", this->setting->langscoreProjectDirectory+"/config.json", "--analyze"});
}

void invoker::write(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", this->setting->langscoreProjectDirectory+"/config.json", "--write"});
}

void invoker::validate(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", this->setting->langscoreProjectDirectory+"/config.json", "--validate"});
}

void invoker::packing(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", this->setting->langscoreProjectDirectory+"/config.json", "--packing"});
}

void invoker::doProcess(QStringList option)
{
    auto process = new QProcess();
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process](){
        QString message = process->readAllStandardOutput();
        emit this->getStdOut(message);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process](){
        QString message = process->readAllStandardError();
        qDebug() << message;
        emit this->getStdOut(message);
    });
    connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [process, this](int exitCode, QProcess::ExitStatus){
        emit this->finish(exitCode);
        process->deleteLater();
    });

    process->start(qApp->applicationDirPath()+"/bin/divisi.exe", option);
    if (!process->waitForStarted(-1)) {
        emit this->getStdOut(process->errorString());
        delete process;
        return;
    }

    if(sync){
        process->waitForFinished(-1);
    }
}
