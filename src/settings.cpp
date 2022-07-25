#include "settings.h"
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>

settings::settings()
    : projectType(settings::VXAce)
    , gameProjectPath("")
    , languages()
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

settings::Language &settings::fetchLanguage(QString bcp47Name)
{
    auto result = std::find(this->languages.begin(), this->languages.end(), bcp47Name);
    if(result != this->languages.end()){
        return *result;
    }
    this->languages.emplace_back(
        settings::Language{ bcp47Name, settings::Font{ "", 22 }, false }
    );
    return this->languages[this->languages.size() - 1];
}

void settings::removeLanguage(QString bcp47Name)
{
    auto result = std::find(this->languages.begin(), this->languages.end(), bcp47Name);
    if(result == this->languages.end()){ return; }
    this->languages.erase(result);
}

QByteArray settings::createJson()
{
    QJsonObject root;

    QJsonArray langs;
    for(auto& l : this->languages){
        QJsonObject langObj;
        langObj["languageName"] = l.languageName;
        langObj["fontName"] = l.font.name;
        langObj["fontSize"] = int(l.font.size);
        langObj["enable"] = bool(l.enable);
        langs.append(langObj);
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
    write["ExportDirectory"] = writeObj.exportDirectory;
    write["ExportByLang"] = writeObj.exportByLanguage;

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

    QFileInfo info(file);
    QDir parentDir = info.dir();
    if(parentDir.exists() == false){
        parentDir.mkdir(parentDir.absolutePath());
    }

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
    auto jsonLanguages = root["Languages"].toArray();
    if(jsonLanguages.empty() == false)
    {
        for(auto l : jsonLanguages)
        {
            auto obj = l.toObject();
            auto data = Language{obj["languageName"].toString(),
                                 Font{obj["fontName"].toString(), static_cast<uint32_t>(obj["fontSize"].toInt())},
                                 obj["enable"].toBool(false)};
            this->languages.emplace_back(data);
        }
    }
    this->defaultLanguage = root["DefaultLanguage"].toString("ja");

    auto write = root["write"].toObject();

    writeObj.exportDirectory = write["ExportDirectory"].toString("");
    writeObj.exportByLanguage = write["ExportByLang"].toBool(false);

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
