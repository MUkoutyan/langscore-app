﻿#include "settings.h"
#include "utility.hpp"
#include "csv.hpp"
#include "application_config.h"
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <assert.h>

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
    PackingInputDir,

    ApplicationVersion,
    ConfigVersion,
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
    MAKE_KEYVALUE(PackingInputDir),
    MAKE_KEYVALUE(ApplicationVersion),
    MAKE_KEYVALUE(ConfigVersion),
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

const static QMap<QString, settings::ProjectType> projectExtensionAndType = {
    {"rvproj2",     settings::VXAce},
    {"rpgproject",  settings::MV},
    {"rmmzproject", settings::MZ},
};

}

settings::settings()
    : projectType(settings::VXAce)
    , gameProjectPath("")
    , languages()
    , defaultLanguage("ja")
    , langscoreProjectDirectory("")
{
}

void settings::setGameProjectPath(QString absoluteGameProjectPath)
{
    absoluteGameProjectPath.replace("\\", "/");

    auto projType = settings::getProjectType(absoluteGameProjectPath);

    if(this->gameProjectPath == absoluteGameProjectPath){ return; }
    if(projType == ProjectType::None){ return; }

    this->gameProjectPath = absoluteGameProjectPath;
    this->projectType = projType;

    if(projType == ProjectType::VXAce){
        this->writeObj.exportDirectory = "./Data/Translate";
    }
    else if(projType == ProjectType::MV || projType == ProjectType::MZ){
        this->writeObj.exportDirectory = "./data/translate";
    }

    updateLangscoreProjectPath();
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

QString settings::analyzeDirectoryPath() const {
    return this->langscoreProjectDirectory+"/analyze";
}

QString settings::tempScriptFileDirectoryPath() const
{
    return this->analyzeDirectoryPath() + "/Scripts";
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

settings::BasicData& settings::fetchBasicDataInfo(QString fileName)
{
    fileName = langscore::withoutAllExtension(fileName);
    if(fileName.isEmpty()){
        throw "Load Invalid Basic Script Info";
    }

    fileName += ".json";
    auto& list = writeObj.basicDataInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& x){
        return x.name == name;
    });

    if(result != list.end()){
        return *result;
    }

    if(fileName.indexOf(".") == -1){
        qDebug() << "Hoge";
    }

    list.emplace_back(
        settings::BasicData{fileName, false, 0}
    );
    return list[list.size() - 1];
}

settings::ScriptInfo &settings::fetchScriptInfo(QString fileName)
{
    fileName = langscore::withoutAllExtension(fileName);
    if(fileName.isEmpty()){
        throw "Load Invalid Script Info";
    }

    fileName += ::scriptExt(projectType);
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

void settings::setPackingDirectory(QString validatePath)
{
    if(validatePath.isEmpty()){
        validatePath = this->writeObj.exportDirectory;
    }

    QFileInfo outputDirInfo(validatePath);
    if(outputDirInfo.isRelative()){
        auto lsProjDir = QDir(this->langscoreProjectDirectory);
        auto absPath = lsProjDir.absoluteFilePath(validatePath);
        absPath = lsProjDir.cleanPath(absPath);
        validatePath = absPath;
    } else {
        validatePath = outputDirInfo.absoluteFilePath();
    }
    this->packingInputDirectory = validatePath;
}

QByteArray settings::createJson()
{
    QJsonObject root;

    root[key(JsonKey::ApplicationVersion)] = PROJECT_VER;
    root[key(JsonKey::ConfigVersion)] = CONFIG_FILE_VERSION;

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
    auto tempPath = QDir(this->langscoreProjectDirectory).relativeFilePath(this->analyzeDirectoryPath());
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
    write[key(JsonKey::WriteType)] = writeObj.writeMode;

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


    root[key(JsonKey::PackingInputDir)] = this->packingInputDirectory;

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

   if(projectType == ProjectType::None){
        QFileInfo lsProjFile(path);
        auto parentDir = lsProjFile.absolutePath();
        parentDir.remove("_langscore");
        this->setGameProjectPath(parentDir);

        if(this->projectType == ProjectType::None){ return; }
    }

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
//    writeObj.writeMode = -1;

    auto basicScripts = write[key(JsonKey::RPGMakerBasicData)].toArray();
    for(auto jsonInfo : basicScripts)
    {
        auto jsonScript = jsonInfo.toObject();

        auto name = jsonScript[key(JsonKey::Name)].toString();

        try{
            auto& info = this->fetchBasicDataInfo(name);
            info.ignore = jsonScript[key(JsonKey::Ignore)].toBool();
        }
        catch(const char* mes){
//            std::cerr << mes << std::endl;
        }
    }

    auto jsonScripts = write[key(JsonKey::RPGMakerScripts)].toArray();
    auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath()+"/_list.csv");
    for(const auto& jsonInfo : jsonScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        auto fileName = jsonScript[key(JsonKey::Name)].toString();

        try
        {
            ScriptInfo& info = this->fetchScriptInfo(fileName);
            //スクリプト情報の名前がスクリプト名だった時の対応。数字列以外が含まれていたらスクリプト名とみなしてIDに変更する。
            if(std::count_if(fileName.cbegin(), fileName.cend(), [](const auto c){
                return c < '0' || '9' < c;
            }) > 0){
                auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&fileName](const auto& x){ return x[1] == fileName; });
                if(result != scriptList.cend()){
                    info.name = (*result)[0];
                }
            }

            info.ignore = jsonScript[key(JsonKey::Ignore)].toBool();

            auto jsonIgnorePoints = jsonScript[key(JsonKey::IgnorePoints)].toArray();
            for(const auto& jsonPoint : jsonIgnorePoints)
            {
                auto obj = jsonPoint.toObject();
                auto pair = TextPoint{
                                static_cast<size_t>(obj[key(JsonKey::Row)].toInteger()),
                                static_cast<size_t>(obj[key(JsonKey::Col)].toInteger()),
                                obj[key(JsonKey::Name)].toString(),
                                obj[key(JsonKey::Disable)].toBool(false),
                                obj[key(JsonKey::Ignore)].toBool(false),
                                obj[key(JsonKey::WriteType)].toInt()
                            };

                auto result = std::find_if(info.ignorePoint.begin(), info.ignorePoint.end(), [&pair](const auto& x){
                    return x == std::pair<size_t,size_t>{pair.row, pair.col};
                });
                if(result == info.ignorePoint.end()){
                    info.ignorePoint.emplace_back(std::move(pair));
                }
            }
        }
        catch(const char* mes){
//            std::cerr << mes << std::endl;
        }
    }

    auto jsonIgnorePictures = write[key(JsonKey::IgnorePictures)].toArray();
    for(auto jsonPath : jsonIgnorePictures)
    {
        this->writeObj.ignorePicturePath.emplace_back(jsonPath.toString());
    }

    this->packingInputDirectory = root[key(JsonKey::PackingInputDir)].toString();
}

void settings::updateLangscoreProjectPath()
{
    //ゲームプロジェクトファイルと同階層に Project_langscore というフォルダを作成する。
    //ゲームプロジェクトと同じフォルダに含めると、アーカイブ作成時にファイルが上手く含まれない場合がある。
    auto folderName = this->gameProjectPath.sliced(this->gameProjectPath.lastIndexOf("/")+1);
    auto gameProjPath = std::filesystem::path(this->gameProjectPath.toLocal8Bit().toStdString());
    auto langscoreProj = gameProjPath / ".." / (folderName.toLocal8Bit().toStdString() + "_langscore");
    auto u8Path = std::filesystem::absolute(langscoreProj).u8string();
    this->langscoreProjectDirectory = QString::fromStdString({u8Path.begin(), u8Path.end()});;
    this->langscoreProjectDirectory.replace("\\", "/");
}

settings::ProjectType settings::getProjectType(const QString& filePath)
{
    QFileInfo info(filePath);
    if(info.isFile() && projectExtensionAndType.contains(info.suffix())){
        return projectExtensionAndType[info.suffix()];
    }
    else if(info.isDir()){
        QDir dir(filePath);
        auto files = dir.entryInfoList();
        if(files.empty()){ return settings::None; }

        for(auto& child : files){
            if(projectExtensionAndType.contains(child.suffix())){
                return projectExtensionAndType[child.suffix()];
            }
        }
    }
    return settings::None;
}
