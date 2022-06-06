#include "invoker.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>

invoker::invoker(settings setting)
    : setting(std::move(setting))
{
}

bool invoker::run()
{
    QJsonObject root;

    QJsonArray langs;
    for(auto& l : setting.languages){
        langs.append(l);
    }
    root["Languages"] = langs;
    root["DefaultLanguage"] = setting.defaultLanguage;
    root["Project"] = setting.project;

    QJsonObject analyze;
    analyze["TmpDir"] = setting.outputDirectory+"/tmp";
    if(QFile::exists(setting.outputDirectory) == false){
        QDir().mkdir(setting.outputDirectory);
    }
//    analyze["RPGMakerOutputPath"] = setting.rpgMakerOutputPath;

    root["analyze"] = analyze;

    QJsonDocument doc(root);
    auto path = setting.outputDirectory+"/config.json";
    {
        QFile file(path);
        if(file.open(QFile::WriteOnly)){
            file.write(doc.toJson());
        }
    }

    process = new QProcess(this);
    connect(process, &QProcess::readyReadStandardOutput, this, [this](){
        QString message = process->readAllStandardOutput();
        qDebug() << message;
        emit this->getStdOut(message);
    });
    process->setProgram(qApp->applicationDirPath()+"/bin/divisi.exe");

    if(setting.write.empty()){
        process->setArguments({"-c", path, "--analyze"});
    }
    else{
        process->setArguments({"-c", path, "--write"});
    }


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
