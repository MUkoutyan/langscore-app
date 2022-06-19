#include "settings.h"
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>

settings::settings()
    : gameProjectPath("")
    , languages({"ja", "en"})
    , defaultLanguage("ja")
    , tempFileOutputDirectory("")
{

}

QString settings::translateDirectoryPath() const {
    return this->gameProjectPath + "/" + this->transFileOutputDirName + "/";
}

QString settings::tempFileDirectoryPath() const {
    return this->tempFileOutputDirectory+"/tmp/";
}

QString settings::tempScriptFileDirectoryPath() const {
    return this->tempFileDirectoryPath() + "Scripts/";
}

void settings::write(QString path)
{
    QJsonObject root;

    QJsonArray langs;
    for(auto& l : this->languages){
        langs.append(l);
    }
    root["Languages"] = langs;
    root["DefaultLanguage"] = this->defaultLanguage;
    root["Project"] = this->gameProjectPath;

    QJsonObject analyze;
    analyze["TmpDir"] = this->tempFileDirectoryPath();
    if(QFile::exists(this->tempFileOutputDirectory) == false){
        QDir().mkdir(this->tempFileOutputDirectory);
    }
//    analyze["RPGMakerOutputPath"] = this->rpgMakerOutputPath;

    root["analyze"] = analyze;

    QJsonObject write;
    write["UsCustomFuncComment"] = "Scripts/{0}#{1},{2}";
    QJsonObject fonts;
    for(const auto& key : writeObj.langFont.keys())
    {
        const auto& font = writeObj.langFont[key];
        QJsonObject fontInfo;
        fontInfo["name"] = font.name;
        fontInfo["size"] = int(font.size);
        fonts[key] = fontInfo;
    }
    write["fonts"] = fonts;

    QJsonArray ignoreScripts;
    for(const auto& info : writeObj.ignoreScriptInfo)
    {
        QJsonObject script;
        script["name"] = info.name;
        script["ignore"] = info.ignore;
        QJsonArray ignorePointArray;
        for(auto& point : info.ignorePoint){
            QJsonObject obj;
            obj["row"] = qint64(point.first);
            obj["col"] = qint64(point.second);
            ignorePointArray.append(obj);
        }
        script["ignorePoints"] = ignorePointArray;
        ignoreScripts.append(script);
    }

    write["RPGMakerIgnoreScripts"] = ignoreScripts;

    root["write"] = write;


    QJsonDocument doc(root);
    {
        QFile file(path);
        if(file.open(QFile::WriteOnly)){
            file.write(doc.toJson());
        }
    }

}

void settings::save(){
    write(this->tempFileOutputDirectory+"/config.json");
}
