#include "settings.h"
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
#include <QVersionNumber>
#include <QApplication>
#include <assert.h>

#include "config_keys.h"

using namespace langscore;

inline const char* key(JsonKey key)
{
    return configKey(key);
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
    , isFirstExported(false)
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

void settings::setupLanguages(const std::vector<QLocale>& locales)
{
    for(const auto& locale : locales)
    {
        auto bcp47 = settings::getLowerBcp47Name(locale);
        if(std::find(this->languages.cbegin(), this->languages.cend(), bcp47) != this->languages.cend()){
            continue;
        }
        bool isDefault = (bcp47 == this->defaultLanguage);
        this->languages.emplace_back(
            settings::Language{bcp47, getDetafultFont(), isDefault }
        );
    }
}

void settings::setupFontList()
{
    //フォントの設定
    QFontDatabase::removeAllApplicationFonts();

    auto fontExtensions = QStringList() << "*.ttf" << "*.otf";
    {
        QFileInfoList files = QDir(qApp->applicationDirPath()+"/resources/fonts").entryInfoList(fontExtensions, QDir::Files);
        for(auto& info : files){
            auto path = info.absoluteFilePath();
            auto fontIndex = QFontDatabase::addApplicationFont(path);
            auto familiesList = QFontDatabase::applicationFontFamilies(fontIndex);
            this->fontIndexList.emplace_back(settings::Global, fontIndex, familiesList, path);
        }
    }
    //プロジェクトに登録されたフォント
    {
        QFileInfoList files = QDir(this->langscoreProjectDirectory+"/Fonts").entryInfoList(fontExtensions, QDir::Files);
        for(auto& info : files){
            auto path = info.absoluteFilePath();
            auto fontIndex = QFontDatabase::addApplicationFont(path);
            auto familiesList = QFontDatabase::applicationFontFamilies(fontIndex);
            this->fontIndexList.emplace_back(settings::Local, fontIndex, familiesList, path);
        }
    }
}

std::vector<settings::Font> settings::createFontList() const
{
    std::vector<settings::Font> fontList;
    settings::Font defaultFont = this->getDetafultFont();
    const auto projType = this->projectType;
    for(const auto& [type, index, family, path] : this->fontIndexList)
    {
        auto familyList = QFontDatabase::applicationFontFamilies(index);
        for(const auto& family : familyList)
        {
            auto font = QFont(family);
            std::uint32_t defaultPixelSize = font.pixelSize();

            auto familyNameLower = family.toLower();
            if(projType == settings::VXAce && familyNameLower.contains("vl gothic")){
                defaultFont.fontData = QFont(family);
                defaultFont.filePath = path;
                defaultFont.name = family;
                defaultPixelSize = this->getDefaultFontSize();
            }
            else if((projType == settings::MV || projType == settings::MZ) && familyNameLower.contains("m+ 1m")) {
                defaultFont.fontData = QFont(family);
                defaultFont.filePath = path;
                defaultFont.name = family;
                defaultPixelSize = this->getDefaultFontSize();
            }
            font.setPixelSize(defaultPixelSize);
            fontList.emplace_back(settings::Font{family, path, font, defaultPixelSize});
        }
    }

    return fontList;
}

settings::Font settings::getDetafultFont() const
{
    settings::Font defaultFont;
    defaultFont.size = this->getDefaultFontSize();

    if(this->projectType == settings::VXAce){
        defaultFont.fontData = QFont("VL Gothic");
        defaultFont.filePath = qApp->applicationDirPath()+"/resources/fonts/VL-Gothic-Regular.ttf";
        defaultFont.name = "VL Gothic";
    }
    else if((this->projectType == settings::MV || this->projectType == settings::MZ)) {
        defaultFont.fontData = QFont("M+ 1m regular");
        defaultFont.filePath = qApp->applicationDirPath()+"/resources/fonts/mplus-1m-regular.ttf";
        defaultFont.name = "M+ 1m regular";
    }

    return defaultFont;
}

settings::TextValidateTypeMap settings::getValidationCsvData(QString fileName)
{
    return this->getValidationCsvDataRef(fileName);
}

settings::TextValidateTypeMap& settings::getValidationCsvDataRef(QString fileName)
{
    auto it = std::ranges::find_if(writeObj.basicDataInfo, [&fileName](const auto& data) {
        return data.name.contains(fileName);
    });
    if(it != writeObj.basicDataInfo.end()) {
        return it->validateInfo;
    }
    static settings::TextValidateTypeMap Invalid{};
    return Invalid;
}

QString settings::getDefaultFontName() const
{
    switch(this->projectType)
    {
    case settings::VXAce:
        return "VL Gothic";
        break;
    case settings::MV:
        return "M+ 1m regular";
        break;
    case settings::MZ:
        return "VL Gothic";
        break;
    default:
        break;
    }

    return "VL Gothic";
}

uint32_t settings::getDefaultFontSize() const
{
    switch(this->projectType)
    {
    case settings::VXAce:
        return 22;
        break;
    case settings::MV:
        return 28;
        break;
    case settings::MZ:
        return 26;
        break;
    default:
        break;
    }

    return 22;
}

QString settings::translateDirectoryPath() const
{
    if(projectType == ProjectType::VXAce){
        return this->gameProjectPath + "/Data/" + this->transFileOutputDirName;
    }
    else if(projectType == ProjectType::MV || projectType == ProjectType::MZ){
        return this->gameProjectPath + "/data/" + this->transFileOutputDirName.toLower();
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

bool settings::isWritedLangscorePlugin() const
{
    if(this->projectType == settings::VXAce)
    {
        auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath()+"/_list.csv");
        return std::find_if(scriptList.cbegin(), scriptList.cend(), [](const auto& x){ return x[1] == "langscore"; }) != scriptList.cend();
    }
    else if(this->projectType == settings::MV || this->projectType == settings::MZ){
        return QFileInfo::exists(this->gameProjectPath + "/js/plugins/Langscore.js");
    }
    return false;
}

bool settings::isWritedLangscoreCustomPlugin() const
{
    if(this->projectType == settings::VXAce)
    {
        auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath()+"/_list.csv");
        return std::find_if(scriptList.cbegin(), scriptList.cend(), [](const auto& x){ return x[1] == "langscore_custom"; }) != scriptList.cend();
    }
    else if(this->projectType == settings::MV || this->projectType == settings::MZ){
        return QFileInfo::exists(this->gameProjectPath + "/js/plugins/LangscoreCustom.js");
    }

    return false;
}

settings::Language& settings::fetchLanguage(QString bcp47Name)
{
    auto result = std::find(this->languages.begin(), this->languages.end(), bcp47Name);
    if(result != this->languages.end()){
        return *result;
    }
    this->languages.emplace_back(
        settings::Language{ bcp47Name, getDetafultFont(), false }
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
        langObj[key(JsonKey::FontPath)] = l.font.filePath;
        langObj[key(JsonKey::FontSize)] = int(l.font.size);
        langObj[key(JsonKey::Enable)] = bool(l.enable);

        QDir baseDir(qApp->applicationDirPath());
        auto relativePath = baseDir.relativeFilePath(l.font.filePath);
        if(relativePath != l.font.filePath){
            langObj[key(JsonKey::FontPath)] = relativePath;
        }
        else{
            QDir baseDir2(langscoreProjectDirectory);
            auto relativePath2 = baseDir2.relativeFilePath(l.font.filePath);
            langObj[key(JsonKey::FontPath)] = relativePath2;
        }

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
    write[key(JsonKey::UsCustomFuncComment)]        = "Scripts/{0}#{1},{2}";
    write[key(JsonKey::ExportDirectory)]            = QDir(this->langscoreProjectDirectory).relativeFilePath(writeObj.exportDirectory);
    write[key(JsonKey::ExportByLang)]               = writeObj.exportByLanguage;
    write[key(JsonKey::OverwriteLangscore)]         = writeObj.overwriteLangscore;
    write[key(JsonKey::OverwriteLangscoreCustom)]   = writeObj.overwriteLangscoreCustom;
    write[key(JsonKey::EnableLanguagePatch)]        = writeObj.enableLanguagePatch;
    write[key(JsonKey::WriteType)]                  = writeObj.writeMode;

    QJsonArray basicDataList;
    for(const auto& info : writeObj.basicDataInfo)
    {
        QJsonObject script;
        script[key(JsonKey::Name)] = info.name;
        script[key(JsonKey::Ignore)] = info.ignore;


        QJsonObject jsonCsvDataList;
        QJsonArray textCategory;
        for(const auto& [typeName, lang_infos] : info.validateInfo)
        {

            QJsonObject typesObj;
            typesObj[key(JsonKey::Name)] = typeName;

            int numEmbededTypes = 0;
            for(const auto& [lang, info] : lang_infos)
            {
                if(info.mode == ValidateTextMode::Ignore) { continue; }
                if(info.count == 0 && info.width == 0) {
                    continue;
                }

                QJsonObject validateTypeObj;
                validateTypeObj[key(JsonKey::ValidateTextMode)] = info.mode;
                QJsonObject validateSizeListObj;
                validateSizeListObj[key(JsonKey::ValidateTextLength)] = info.count;
                validateSizeListObj[key(JsonKey::ValidateTextWidth)] = info.width;
                validateTypeObj[key(JsonKey::ValidateSizeList)] = validateSizeListObj;

                typesObj[lang] = validateTypeObj;
                numEmbededTypes++;
            }
            if(0 < numEmbededTypes){
                textCategory.append(typesObj);
            }
        }
        if(textCategory.empty() == false) {
            script[key(JsonKey::ValidateTextCategory)] = textCategory;
        }

        basicDataList.append(script);
    }

    write[key(JsonKey::RPGMakerBasicData)] = basicDataList;
    write[key(JsonKey::IsFirstExported)]   = this->isFirstExported;

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

    root[key(JsonKey::PackingInputDir)] = this->packingInputDirectory;

    QJsonObject validateJsonObjs;
    validateJsonObjs[key(JsonKey::ValidateTextMode)] = static_cast<int>(this->validateObj.textInfo.mode);
    {
        QJsonObject validationCountObj;
        validationCountObj[key(JsonKey::ValidateTextLength)] = this->validateObj.textInfo.count;
        validationCountObj[key(JsonKey::ValidateTextWidth)] = this->validateObj.textInfo.width;
        validateJsonObjs[key(JsonKey::ValidateSizeList)] = validationCountObj;
    }

    //QJsonArray jsonCsvDataList;
    //for(const auto& csvData : validateObj.csvDataList) {
    //    QJsonObject csvDataObj;
    //    csvDataObj[key(JsonKey::Name)] = csvData.name;

    //    QJsonObject typesObj;
    //    int numEmbededTypes = 0;
    //    for(auto it = csvData.types.begin(); it != csvData.types.end(); ++it) 
    //    {
    //        if(it.value().mode == ValidateTextMode::Ignore) { continue; }
    //        if(it.value().validationCount == 0 && it.value().validationWidth == 0) {
    //            continue;
    //        }

    //        QJsonObject validateTypeObj;
    //        validateTypeObj[key(JsonKey::ValidateTextMode)] = it.value().mode;
    //        QJsonObject validateSizeListObj;
    //        validateSizeListObj[key(JsonKey::ValidateTextLength)] = it.value().validationCount;
    //        validateSizeListObj[key(JsonKey::ValidateTextWidth)] = it.value().validationWidth;
    //        validateTypeObj[key(JsonKey::ValidateSizeList)] = validateSizeListObj;
    //        typesObj[it.key()] = validateTypeObj;
    //        numEmbededTypes++;
    //    }
    //    if(0 < numEmbededTypes) 
    //    {
    //        csvDataObj[key(JsonKey::ValidateTextCategory)] = typesObj;
    //        jsonCsvDataList.append(csvDataObj);
    //    }
    //}
    //validateJsonObjs[key(JsonKey::ValidateCSVList)] = jsonCsvDataList;

    root[key(JsonKey::Validate)] = validateJsonObjs;

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


    auto projctVer = root[key(JsonKey::ApplicationVersion)].toString();
    auto configVer = root[key(JsonKey::ConfigVersion)].toString();


    this->languages.clear();
    auto jsonLanguages = root[key(JsonKey::Languages)].toArray();
    if(jsonLanguages.empty() == false)
    {
        if(QVersionNumber::fromString(configVer) <= QVersionNumber::fromString("1.0.0"))
        {
            for(auto l : jsonLanguages)
            {
                auto obj = l.toObject();
                QFont fontData(obj[key(JsonKey::FontName)].toString());
                fontData.setPixelSize(obj[key(JsonKey::FontSize)].toInt());
                auto data = Language{obj[key(JsonKey::LanguageName)].toString(),
                                     Font{ obj[key(JsonKey::FontName)].toString(),
                                           obj[key(JsonKey::FontPath)].toString(),
                                           fontData,
                                           static_cast<uint32_t>(obj[key(JsonKey::FontSize)].toInt())
                                     },
                                     obj[key(JsonKey::Enable)].toBool(false)};
                this->languages.emplace_back(data);
            }
            //フォント再登録のメッセージを出す？
        }
        else
        {
            for(auto l : jsonLanguages)
            {
                auto obj = l.toObject();
                QFont fontData(obj[key(JsonKey::FontName)].toString());
                fontData.setPixelSize(obj[key(JsonKey::FontSize)].toInt());
                auto data = Language{obj[key(JsonKey::LanguageName)].toString(),
                                     Font{ obj[key(JsonKey::FontName)].toString(),
                                           obj[key(JsonKey::FontPath)].toString(),
                                           fontData,
                                           static_cast<uint32_t>(obj[key(JsonKey::FontSize)].toInt())
                                     },
                                     obj[key(JsonKey::Enable)].toBool(false)};
                this->languages.emplace_back(data);
            }
        }
    }
    this->defaultLanguage = root[key(JsonKey::DefaultLanguage)].toString("ja");

    auto write = root[key(JsonKey::Write)].toObject();

    writeObj.exportDirectory     = write[key(JsonKey::ExportDirectory)].toString("");
    writeObj.exportByLanguage    = write[key(JsonKey::ExportByLang)].toBool(false);
    writeObj.enableLanguagePatch = write[key(JsonKey::EnableLanguagePatch)].toBool(false);
    //保存したいようなフラグではないため、設定値を読み込まない
//    writeObj.isWriteCSV               = write[key(JsonKey::IsWriteCsv)].toBool(false);
//    writeObj.overwriteLangscore       = write[key(JsonKey::OverwriteLangscore)].toBool(false);
//    writeObj.overwriteLangscoreCustom = write[key(JsonKey::OverwriteLangscoreCustom)].toBool(false);
//    writeObj.writeMode = -1;

    this->isFirstExported = write[key(JsonKey::IsFirstExported)].toBool();

    auto basicScripts = write[key(JsonKey::RPGMakerBasicData)].toArray();
    for(auto jsonInfo : basicScripts)
    {
        auto jsonScript = jsonInfo.toObject();

        auto name = jsonScript[key(JsonKey::Name)].toString();
        auto isIgnore = jsonScript[key(JsonKey::Ignore)].toBool();

        try{
            auto& info = this->fetchBasicDataInfo(name);
            info.ignore = isIgnore;
            info.name = name;

            if(jsonScript.contains(key(JsonKey::ValidateTextCategory)) && jsonScript[key(JsonKey::ValidateTextCategory)].isArray())
            {
                auto typesArray = jsonScript[key(JsonKey::ValidateTextCategory)].toArray();
                for(const auto& valueTypesData : typesArray)
                {
                    if(valueTypesData.isObject() == false) { continue; }

                    auto valueTypes = valueTypesData.toObject();
                    auto keys = valueTypes.keys();
                    QString typeName = "";
                    TextValidateTypeMap validateTypeMap;
                    TextValidationLangMap langMap;
                    for(const auto& _key : keys)
                    {
                        if(_key == key(JsonKey::Name)) 
                        {
                            typeName = valueTypes[_key].toString();
                        }
                        else 
                        {
                            if(langMap.find(_key) == langMap.end()) {
                                langMap[_key] = {};
                            }

                            auto langValue = valueTypes[_key].toObject();

                            ValidateTextInfo validateType;
                            validateType.mode = static_cast<ValidateTextMode>(langValue[key(JsonKey::ValidateTextMode)].toInt());
                            if(langValue.contains(key(JsonKey::ValidateSizeList)) && langValue[key(JsonKey::ValidateSizeList)].isObject()) {
                                QJsonObject validateSizeListObj = langValue[key(JsonKey::ValidateSizeList)].toObject();
                                validateType.count = validateSizeListObj[key(JsonKey::ValidateTextLength)].toInt();
                                validateType.width = validateSizeListObj[key(JsonKey::ValidateTextWidth)].toInt();
                            }
                            langMap[_key] = validateType;
                        }
                    }

                    if(info.validateInfo.find(typeName) == info.validateInfo.end()) {
                        info.validateInfo[typeName] = std::move(langMap);
                    }
                }
            }
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

    auto validateJsonObj = root[key(JsonKey::Validate)];
    this->validateObj.textInfo.mode = static_cast<ValidateTextMode>(validateJsonObj[key(JsonKey::ValidateTextMode)].toInt());
    {
        auto numberObject = validateJsonObj[key(JsonKey::ValidateSizeList)];
        this->validateObj.textInfo.count = numberObject[key(JsonKey::ValidateTextLength)].toInt();
        this->validateObj.textInfo.width = numberObject[key(JsonKey::ValidateTextWidth)].toInt();
    }

    //auto jsonCsvDataList = validateJsonObj[key(JsonKey::ValidateCSVList)].toArray();
    //for(auto csvDataValue : jsonCsvDataList)
    //{
    //    auto obj = csvDataValue.toObject();
    //    ValidationInfoList data;
    //    data.name = obj[key(JsonKey::Name)].toString();

    //    if(obj.contains(key(JsonKey::ValidateTextCategory)) && obj[key(JsonKey::ValidateTextCategory)].isObject()) {
    //        QJsonObject typesObj = obj[key(JsonKey::ValidateTextCategory)].toObject();
    //        for(auto it = typesObj.begin(); it != typesObj.end(); ++it) {
    //            if(!it.value().isObject()) continue;
    //            QJsonObject validateTypeObj = it.value().toObject();

    //            ValidateTextInfo validateType;
    //            validateType.mode = static_cast<ValidateTextMode>(validateTypeObj[key(JsonKey::ValidateTextMode)].toInt());
    //            if(validateTypeObj.contains(key(JsonKey::ValidateSizeList)) && validateTypeObj[key(JsonKey::ValidateSizeList)].isObject()) {
    //                QJsonObject validateSizeListObj = validateTypeObj[key(JsonKey::ValidateSizeList)].toObject();
    //                validateType.count = validateSizeListObj[key(JsonKey::ValidateTextLength)].toInt();
    //                validateType.width = validateSizeListObj[key(JsonKey::ValidateTextWidth)].toInt();
    //            }
    //            data.types[it.key()] = validateType;
    //        }
    //    }

    //    this->validateObj.csvDataList.emplace_back(std::move(data));
    //}

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
