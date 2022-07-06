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

QString settings::tempGraphicsFileDirectoryPath() const
{
    return this->gameProjectPath + "/Graphics/Pictures/";
}

QByteArray settings::createJson()
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


    QJsonArray ignorePictures;
    for(const auto& path : writeObj.ignorePicturePath){
        ignorePictures.append(path);
    }
    write["ignorePictures"] = ignorePictures;

    root["write"] = write;


    QJsonDocument doc(root);
    return doc.toJson();
}

void settings::write(QString path)
{
    QFile file(path);
    if(file.open(QFile::WriteOnly)){
        file.write(createJson());
    }
}

void settings::saveForProject(){
    write(this->tempFileOutputDirectory+"/config.json");
}

void settings::load(QString path)
{
    QFile file(path);
    if(file.open(QFile::ReadOnly) == false){ return; }

    QJsonDocument root = QJsonDocument::fromJson(QByteArray(file.readAll()));

    this->languages.clear();
    auto jsonLanguages = root["Languages"].toArray({"ja", "en"});
    for(auto l : jsonLanguages){
        this->languages.emplace_back(l.toString());
    }
    this->defaultLanguage = root["DefaultLanguage"].toString("ja");

    auto write = root["write"].toObject();
    auto jsonFonts = write["fonts"].toObject();
    QMap<QString, WriteProps::Font> fonts;
    for(const auto& key : jsonFonts.keys())
    {
        auto f = jsonFonts[key].toObject();
        WriteProps::Font font;
        font.name = f["name"].toString();
        font.name = f["size"].toString();
        fonts[key] = font;
    }

    auto jsonIgnoreScripts = write["RPGMakerIgnoreScripts"].toArray();
    for(auto jsonInfo : jsonIgnoreScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        WriteProps::ScriptInfo info;
        info.name = jsonScript["name"].toString();
        info.ignore = jsonScript["ignore"].toBool();

        auto jsonIgnorePoints = jsonScript["ignorePoints"].toArray();
        for(auto jsonPoint : jsonIgnorePoints){
            auto obj = jsonPoint.toObject();
            auto pair = std::pair<size_t, size_t>{obj["row"].toInteger(), obj["col"].toInteger()};
            info.ignorePoint.emplace_back(std::move(pair));
        }
        this->writeObj.ignoreScriptInfo.emplace_back(std::move(info));
    }

    auto jsonIgnorePictures = write["ignorePictures"].toArray();
    for(auto jsonPath : jsonIgnorePictures)
    {
        this->writeObj.ignorePicturePath.emplace_back(jsonPath.toString());
    }


}
