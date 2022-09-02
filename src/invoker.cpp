#include "invoker.h"
#include <QApplication>
#include <QFile>
#include <QDialog>
#include <QVBoxLayout>

invoker::invoker(ComponentBase *setting)
    : ComponentBase(setting){
}

int invoker::analyze()
{
    return doProcess({"-c", this->setting->langscoreProjectDirectory+"/config.json", "--analyze"});
}

int invoker::write()
{
    return doProcess({"-c", this->setting->langscoreProjectDirectory+"/config.json", "--write"});
}

int invoker::doProcess(QStringList option)
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

    while(process->waitForFinished(200)){
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        emit this->update();
    }

    auto code = process->exitCode();
    if(code == 0){
        emit this->getStdOut(" Ok : " + QString(process->readAllStandardOutput()) + "\n" + process->errorString());
    }
    else
    {
        emit this->getStdOut("Error. code " + QString::number(code));
        switch(code)
        {
        case -1:

            break;
        }
    }

    emit this->getStdOut(QString(process->readAllStandardOutput()));


    return code;
}
