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
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--analyze"});
}

void invoker::updateData(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--update"});
}

void invoker::write(bool sync)
{
    this->sync = sync;
    return doProcess({"-c", "\""+this->setting->langscoreProjectDirectory+"/config.json\"", "--write"});
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
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process](){
        QString message = QString::fromLocal8Bit(process->readAllStandardOutput());
        emit this->getStdOut(message);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process](){
        QString message = QString::fromLocal8Bit(process->readAllStandardError());
        emit this->getStdOut(message);
    });
    connect(process, static_cast<void (QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished), this, [process, this](int exitCode, QProcess::ExitStatus status){
        emit this->finish(exitCode, status);
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
