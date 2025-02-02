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
    connect(process, &QProcess::readyReadStandardOutput, this, [this, process](){
        QString message = QString::fromUtf8(process->readAllStandardOutput());
        emit this->getStdOut(message);
    });
    connect(process, &QProcess::readyReadStandardError, this, [this, process](){
        QString message = QString::fromUtf8(process->readAllStandardError());
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
        return;
    }

    if(sync){
        process->waitForFinished(-1);
    }
}
