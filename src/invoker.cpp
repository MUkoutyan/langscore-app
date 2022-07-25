#include "invoker.h"
#include <QApplication>
#include <QFile>
#include <QDialog>
#include <QVBoxLayout>

invoker::invoker(ComponentBase *setting)
    : ComponentBase(setting){
}

bool invoker::analyze()
{
    auto path = this->setting->tempFileOutputDirectory+"/tmp.json";
    this->setting->write(path);
    auto result = doProcess({"-c", path, "--analyze"});
    QFile::remove(path);
    return result;
}

bool invoker::write()
{
    auto path = this->setting->tempFileOutputDirectory+"/config.json";
    this->setting->write(path);
    return doProcess({"-c", path, "--write"});
}

bool invoker::doProcess(QStringList option)
{
    process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardOutput, this, [this](){
        QString message = process->readAllStandardOutput();
        qDebug() << message;
        emit this->getStdOut(message);
    });
    process->setProgram(qApp->applicationDirPath()+"/bin/divisi.exe");

    process->setArguments(option);

    process->start();
    if (!process->waitForStarted(-1)) {
        emit this->getStdOut(process->errorString());
    }

    process->waitForFinished(-1);

    auto code = process->exitCode();
    if(code == 0){
        emit this->getStdOut(" Ok : " + QString(process->readAllStandardOutput()) + "\n" + process->errorString());
    }
    else
    {
        emit this->getStdOut("Error.");
        switch(code)
        {
        case -1:

            break;
        }
    }

    emit this->getStdOut(QString(process->readAllStandardOutput()));


    return code == 0;
}
