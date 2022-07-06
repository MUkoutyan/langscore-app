#include "invoker.h"
#include <QApplication>
#include <QFile>

invoker::invoker(Common::Type setting)
    : ComponentBase(std::move(setting)){
}

bool invoker::analyze()
{
    auto path = this->common.obj->tempFileOutputDirectory+"/tmp.json";
    this->common.obj->write(path);
    return doProcess({"-c", path, "--analyze"});
}

bool invoker::write()
{
    auto path = this->common.obj->tempFileOutputDirectory+"/tmp.json";
    this->common.obj->write(path);
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
        qWarning() << process->errorString();
    }

    process->waitForFinished(-1);

    if(process->exitCode()!=0)
    {
        qDebug () << " Error " << process->exitCode() << process->errorString();
        return false;
    }
    else
    {
        qDebug () << " Ok " <<  QString(process->readAllStandardOutput()) <<  process->errorString();
    }

    qDebug() << QString(process->readAllStandardOutput());


    return true;
}
