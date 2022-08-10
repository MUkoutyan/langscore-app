#include "settings.h"
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>

#define MAKE_KEYVALUE(k) {JsonKey::k, #k}

enum class JsonKey : size_t
{
    Languages = 0i64,
    LanguageName,
    Enable,
    Disable,
    FontName,
    FontSize,
    Project,
    Analyze,
    Write,
    Name,
    Ignore,
    IgnorePoints,
    Row,
    Col,
    WriteType,
    Text,
    IgnorePictures,
    DefaultLanguage,
    TmpDir,
    UsCustomFuncComment,
    ExportDirectory,
    ExportByLang,
    RPGMakerOutputPath,
    RPGMakerBasicData,
    RPGMakerScripts,

    NumKeys,
};

static const std::map<JsonKey, const char*> jsonKeys = {
    MAKE_KEYVALUE(Languages),
    MAKE_KEYVALUE(LanguageName),
    MAKE_KEYVALUE(Enable),
    MAKE_KEYVALUE(Disable),
    MAKE_KEYVALUE(FontName),
    MAKE_KEYVALUE(FontSize),
    MAKE_KEYVALUE(Project),
    MAKE_KEYVALUE(Analyze),
    MAKE_KEYVALUE(Write),
    MAKE_KEYVALUE(Name),
    MAKE_KEYVALUE(Ignore),
    MAKE_KEYVALUE(IgnorePoints),
    MAKE_KEYVALUE(Row),
    MAKE_KEYVALUE(Col),
    MAKE_KEYVALUE(WriteType),
    MAKE_KEYVALUE(Text),
    MAKE_KEYVALUE(IgnorePictures),
    MAKE_KEYVALUE(DefaultLanguage),
    MAKE_KEYVALUE(TmpDir),
    MAKE_KEYVALUE(UsCustomFuncComment),
    MAKE_KEYVALUE(ExportDirectory),
    MAKE_KEYVALUE(ExportByLang),
    MAKE_KEYVALUE(RPGMakerOutputPath),
    MAKE_KEYVALUE(RPGMakerBasicData),
    MAKE_KEYVALUE(RPGMakerScripts),
};

inline const char* key(JsonKey key)
{
    assert(jsonKeys.size() == size_t(JsonKey::NumKeys));
    if(jsonKeys.find(key) == jsonKeys.end()){ return nullptr; }
    return jsonKeys.at(key);
}

namespace {
QString scriptExt(settings::ProjectType type){
    return type==settings::ProjectType::VXAce?".rb":".js";
}
}

settings::settings()
    : projectType(settings::VXAce)
    , gameProjectPath("")
    , languages()
    , defaultLanguage("ja")
    , langscoreProjectDirectory("")
{

}

QString settings::translateDirectoryPath() const {
    return this->gameProjectPath + "/" + this->transFileOutputDirName;
}

QString settings::tempFileDirectoryPath() const {
    return this->langscoreProjectDirectory+"/analyze";
}

QString settings::tempScriptFileDirectoryPath() const {
    return this->tempFileDirectoryPath() + "/Scripts";
}

QString settings::tempGraphicsFileDirectoryPath() const
{
    return this->gameProjectPath + "/Graphics";
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

settings::ScriptInfo &settings::fetchScriptInfo(QString fileName)
{
    fileName = QFileInfo(fileName).completeBaseName() + ::scriptExt(projectType);
    auto& list = writeObj.scriptInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& script){
        return script.name == name;
    });

    if(result != list.end()){
        return *result;
    }

    list.emplace_back(
        settings::ScriptInfo{{fileName, false, 0}, {}}
    );
    return list[list.size() - 1];
}

void settings::removeScriptInfoPoint(QString fileName, std::pair<size_t,size_t> point)
{
    fileName = QFileInfo(fileName).completeBaseName() + ::scriptExt(projectType);
    auto& list = writeObj.scriptInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& script){
        return script.name == name;
    });
    if(result != list.end()){
        auto rm_result = std::remove(result->ignorePoint.begin(), result->ignorePoint.end(), point);
        result->ignorePoint.erase(rm_result, result->ignorePoint.end());
    }
}

QByteArray settings::createJson()
{
    QJsonObject root;

    QJsonArray langs;
    for(auto& l : this->languages){
        QJsonObject langObj;
        langObj[key(JsonKey::LanguageName)] = l.languageName;
        langObj[key(JsonKey::FontName)] = l.font.name;
        langObj[key(JsonKey::FontSize)] = int(l.font.size);
        langObj[key(JsonKey::Enable)] = bool(l.enable);
        langs.append(langObj);
    }
    root[key(JsonKey::Languages)] = langs;
    root[key(JsonKey::DefaultLanguage)] = this->defaultLanguage;
    root[key(JsonKey::Project)] = "./" + QDir(this->langscoreProjectDirectory).relativeFilePath(this->gameProjectPath);

    QJsonObject analyze;
    auto tempPath = QDir(this->langscoreProjectDirectory).relativeFilePath(this->tempFileDirectoryPath());
    analyze[key(JsonKey::TmpDir)] = "./" + tempPath;
    if(QFile::exists(this->langscoreProjectDirectory) == false){
        QDir().mkdir(this->langscoreProjectDirectory);
    }

    root[key(JsonKey::Analyze)] = analyze;

    QJsonObject write;
    write[key(JsonKey::UsCustomFuncComment)] = "Scripts/{0}#{1},{2}";
    write[key(JsonKey::ExportDirectory)] = QDir(this->langscoreProjectDirectory).relativeFilePath(writeObj.exportDirectory);
    write[key(JsonKey::ExportByLang)] = writeObj.exportByLanguage;


    QJsonArray basicDataList;
    for(const auto& info : writeObj.basicDataInfo)
    {
        QJsonObject script;
        script[key(JsonKey::Name)] = info.name;
        script[key(JsonKey::Ignore)] = info.ignore;
        basicDataList.append(script);
    }

    write[key(JsonKey::RPGMakerBasicData)] = basicDataList;

    QJsonArray scripts;
    for(const auto& info : writeObj.scriptInfo)
    {
        QJsonObject script;
        script[key(JsonKey::Name)] = info.name;
        script[key(JsonKey::Ignore)] = info.ignore;
        QJsonArray ignorePointArray;
        for(auto& point : info.ignorePoint){
            QJsonObject obj;
            obj[key(JsonKey::Row)] = qint64(point.row);
            obj[key(JsonKey::Col)] = qint64(point.col);
            obj[key(JsonKey::Disable)] = false;
            script[key(JsonKey::Ignore)] = point.ignore;
            ignorePointArray.append(obj);
        }
        script[key(JsonKey::IgnorePoints)] = ignorePointArray;
        scripts.append(script);
    }

    write[key(JsonKey::RPGMakerScripts)] = scripts;


    QJsonArray ignorePictures;
    for(const auto& path : writeObj.ignorePicturePath){
        ignorePictures.append(path);
    }
    write[key(JsonKey::IgnorePictures)] = ignorePictures;

    root[key(JsonKey::Write)] = write;

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
    write(this->langscoreProjectDirectory+"/config.json");
}

void settings::load(QString path)
{
    QFile file(path);
    if(file.open(QFile::ReadOnly) == false){ return; }

    QJsonDocument root = QJsonDocument::fromJson(QByteArray(file.readAll()));

    this->languages.clear();
    auto jsonLanguages = root[key(JsonKey::Languages)].toArray();
    if(jsonLanguages.empty() == false)
    {
        for(auto l : jsonLanguages)
        {
            auto obj = l.toObject();
            auto data = Language{obj[key(JsonKey::LanguageName)].toString(),
                                 Font{obj[key(JsonKey::FontName)].toString(), static_cast<uint32_t>(obj[key(JsonKey::FontSize)].toInt())},
                                 obj[key(JsonKey::Enable)].toBool(false)};
            this->languages.emplace_back(data);
        }
    }
    this->defaultLanguage = root[key(JsonKey::DefaultLanguage)].toString("ja");

    auto write = root[key(JsonKey::Write)].toObject();

    writeObj.exportDirectory = write[key(JsonKey::ExportDirectory)].toString("");
    writeObj.exportByLanguage = write[key(JsonKey::ExportByLang)].toBool(false);

    auto jsonScripts = write[key(JsonKey::RPGMakerScripts)].toArray();
    for(auto jsonInfo : jsonScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        ScriptInfo info;
        info.name = jsonScript[key(JsonKey::Name)].toString();
        info.ignore = jsonScript[key(JsonKey::Ignore)].toBool();

        auto jsonIgnorePoints = jsonScript[key(JsonKey::IgnorePoints)].toArray();
        for(auto jsonPoint : jsonIgnorePoints){
            auto obj = jsonPoint.toObject();
            auto pair = TextPoint{
                            static_cast<size_t>(obj[key(JsonKey::Row)].toInteger()),
                            static_cast<size_t>(obj[key(JsonKey::Col)].toInteger()),
                            obj[key(JsonKey::Disable)].toBool(false),
                            obj[key(JsonKey::Ignore)].toBool(false),
                            obj[key(JsonKey::WriteType)].toInt()
                        };
            info.ignorePoint.emplace_back(std::move(pair));
        }
        this->writeObj.scriptInfo.emplace_back(std::move(info));
    }

    auto jsonIgnorePictures = write[key(JsonKey::IgnorePictures)].toArray();
    for(auto jsonPath : jsonIgnorePictures)
    {
        this->writeObj.ignorePicturePath.emplace_back(jsonPath.toString());
    }


}
