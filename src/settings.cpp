#include "settings.h"
#include "utility.hpp"
#include "csv.hpp"
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFontDatabase>

#define MAKE_KEYVALUE(k) {JsonKey::k, #k}

enum class JsonKey : size_t
{
    Languages = 0i64,
    LanguageName,
    Enable,
    Disable,
    FontName,
    FontSize,
    FontPath,
    Global,
    Local,
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
    OverwriteLangscore,
    OverwriteLangscoreCustom,

    NumKeys,
};

static const std::map<JsonKey, const char*> jsonKeys = {
    MAKE_KEYVALUE(Languages),
    MAKE_KEYVALUE(LanguageName),
    MAKE_KEYVALUE(Enable),
    MAKE_KEYVALUE(Disable),
    MAKE_KEYVALUE(FontName),
    MAKE_KEYVALUE(FontSize),
    MAKE_KEYVALUE(FontPath),
    MAKE_KEYVALUE(Global),
    MAKE_KEYVALUE(Local),
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
    MAKE_KEYVALUE(OverwriteLangscore),
    MAKE_KEYVALUE(OverwriteLangscoreCustom),
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

void settings::setGameProjectPath(QString absolutePath)
{
    absolutePath.replace("\\", "/");
    this->gameProjectPath = absolutePath;

    //ゲームプロジェクトファイルと同階層に Project_langscore というフォルダを作成する。
    //ゲームプロジェクトと同じフォルダに含めると、アーカイブ作成時にファイルが上手く含まれない場合がある。
    auto folderName = absolutePath.sliced(absolutePath.lastIndexOf("/")+1);
    auto gameProjPath = std::filesystem::path(absolutePath.toLocal8Bit().toStdString());
    auto langscoreProj = gameProjPath / ".." / (folderName.toLocal8Bit().toStdString() + "_langscore");
    auto u8Path = std::filesystem::absolute(langscoreProj).u8string();
    this->langscoreProjectDirectory = QString::fromStdString({u8Path.begin(), u8Path.end()});;
    this->langscoreProjectDirectory.replace("\\", "/");
}

QString settings::translateDirectoryPath() const
{
    if(projectType == ProjectType::VXAce){
        return this->gameProjectPath + "/Data/" + this->transFileOutputDirName;
    }
    else if(projectType == ProjectType::MV || projectType == ProjectType::MZ){
        return this->gameProjectPath + "/data/" + this->transFileOutputDirName;
    }

    return this->gameProjectPath + "/data/" + this->transFileOutputDirName;
}

QString settings::tempFileDirectoryPath() const {
    return this->langscoreProjectDirectory+"/analyze";
}

QString settings::tempScriptFileDirectoryPath() const
{
    return this->tempFileDirectoryPath() + "/Scripts";
}

QString settings::tempGraphicsFileDirectoryPath() const
{
    if(projectType == ProjectType::VXAce){
        return this->gameProjectPath + "/Graphics";
    }
    else if(projectType == ProjectType::MV || projectType == ProjectType::MZ){
        return this->gameProjectPath + "/img";
    }
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

settings::BasicData &settings::fetchBasicDataInfo(QString fileName)
{
    fileName = QFileInfo(fileName).completeBaseName() + ".json";
    auto& list = writeObj.basicDataInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& x){
        return x.name == name;
    });

    if(result != list.end()){
        return *result;
    }

    list.emplace_back(
        settings::BasicData{fileName, false, 0}
    );
    return list[list.size() - 1];
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

QString settings::getLowerBcp47Name(QLocale locale)
{
    auto bcp47Name = locale.bcp47Name().toLower();
    if(bcp47Name == "zh"){ bcp47Name = "zh-cn"; }
    return bcp47Name;
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
    write[key(JsonKey::OverwriteLangscore)] = writeObj.overwriteLangscore;
    write[key(JsonKey::OverwriteLangscoreCustom)] = writeObj.overwriteLangscoreCustom;

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

    std::vector<QString> copyGlobalFontPathList;
    std::vector<QString> copyLocalFontPathList;
    for(const auto& fontInfo : this->fontIndexList)
    {
        auto [type, index, path] = fontInfo;
        auto familyList = QFontDatabase::applicationFontFamilies(index);
        for(const auto& family : familyList)
        {
            auto familyName = family.toLower();
            if(projectType == settings::VXAce && familyName.contains("vl gothic")){
                continue;
            }
            else if((projectType == settings::MV || projectType == settings::MZ) && familyName.contains("m+ 1m")) {
                continue;
            }

            auto result = std::find_if(this->languages.cbegin(), this->languages.cend(), [&familyName](const auto& x){
                return x.enable && x.font.name.toLower() == familyName;
            });
            if(result != this->languages.cend()){
                if(type == settings::Global){
                    copyGlobalFontPathList.emplace_back(path);
                } else if(type == settings::Local){
                    copyLocalFontPathList.emplace_back(path);
                }
                break;
            }
        }
    }
    QJsonArray copyGlobalFonts;
    QJsonArray copyLocalFonts;
    for(const auto& path : copyGlobalFontPathList){ copyGlobalFonts.append(path); }
    for(const auto& path : copyLocalFontPathList){ copyLocalFonts.append(path); }
    QJsonObject copyFonts;
    copyFonts[key(JsonKey::Global)] = copyGlobalFonts;
    copyFonts[key(JsonKey::Local)] = copyLocalFonts;
    write[key(JsonKey::FontPath)] = copyFonts;

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
                                 Font{ obj[key(JsonKey::FontName)].toString(),
                                       static_cast<uint32_t>(obj[key(JsonKey::FontSize)].toInt())
                                 },
                                 obj[key(JsonKey::Enable)].toBool(false)};
            this->languages.emplace_back(data);
        }
    }
    this->defaultLanguage = root[key(JsonKey::DefaultLanguage)].toString("ja");

    auto write = root[key(JsonKey::Write)].toObject();

    writeObj.exportDirectory = write[key(JsonKey::ExportDirectory)].toString("");
    writeObj.exportByLanguage = write[key(JsonKey::ExportByLang)].toBool(false);
    //保存したいようなフラグではないため、設定値を読み込まない
//    writeObj.overwriteLangscore = write[key(JsonKey::OverwriteLangscore)].toBool(false);
//    writeObj.overwriteLangscoreCustom = write[key(JsonKey::OverwriteLangscoreCustom)].toBool(false);

    auto basicScripts = write[key(JsonKey::RPGMakerBasicData)].toArray();
    for(auto jsonInfo : basicScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        BasicData info;
        info.name = jsonScript[key(JsonKey::Name)].toString();
        info.ignore = jsonScript[key(JsonKey::Ignore)].toBool();
        this->writeObj.basicDataInfo.emplace_back(std::move(info));
    }

    auto jsonScripts = write[key(JsonKey::RPGMakerScripts)].toArray();
    auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath()+"/_list.csv");
    for(auto jsonInfo : jsonScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        auto fileName = jsonScript[key(JsonKey::Name)].toString();

        fileName = langscore::withoutExtension(fileName);

        ScriptInfo& info = this->fetchScriptInfo(fileName);
        if(std::count_if(fileName.cbegin(), fileName.cend(), [](const auto c){
            return c < '0' || '9' < c;
        }) > 0){
            auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&fileName](const auto& x){ return x[1] == fileName; });
            if(result != scriptList.cend()){
                info.name = (*result)[0];
            }
        }
        else {
            info.name = fileName;
        }

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
//        this->writeObj.scriptInfo.emplace_back(std::move(info));
    }

    auto jsonIgnorePictures = write[key(JsonKey::IgnorePictures)].toArray();
    for(auto jsonPath : jsonIgnorePictures)
    {
        this->writeObj.ignorePicturePath.emplace_back(jsonPath.toString());
    }


}
