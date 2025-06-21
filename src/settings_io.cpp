#include "settings.h"
#include "application_config.h"
#include "config_keys.h"
#include "csv.hpp"

#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QApplication>
#include <QVersionNumber>

using namespace langscore;

inline const char* key(JsonKey key)
{
    return configKey(key);
}

QByteArray settings::createJson()
{
    QJsonObject root;

    root[key(JsonKey::ApplicationVersion)] = PROJECT_VER;
    root[key(JsonKey::ConfigVersion)] = CONFIG_FILE_VERSION;

    QJsonArray langs;
    for(auto& l : this->languages) {
        QJsonObject langObj;
        langObj[key(JsonKey::LanguageName)] = l.languageName;
        langObj[key(JsonKey::FontName)] = l.font.name;
        langObj[key(JsonKey::FontPath)] = l.font.filePath;
        langObj[key(JsonKey::FontSize)] = int(l.font.size);
        langObj[key(JsonKey::Enable)] = bool(l.enable);

        QDir baseDir(qApp->applicationDirPath());
        auto relativePath = baseDir.relativeFilePath(l.font.filePath);
        if(relativePath != l.font.filePath) {
            langObj[key(JsonKey::FontPath)] = relativePath;
        }
        else {
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
    if(QFile::exists(this->langscoreProjectDirectory) == false) {
        QDir().mkdir(this->langscoreProjectDirectory);
    }

    root[key(JsonKey::Analyze)] = analyze;

    QJsonObject write;
    write[key(JsonKey::UsCustomFuncComment)] = "Scripts/{0}#{1},{2}";
    write[key(JsonKey::ExportDirectory)] = QDir(this->langscoreProjectDirectory).relativeFilePath(writeObj.exportDirectory);
    write[key(JsonKey::ExportByLang)] = writeObj.exportByLanguage;
    write[key(JsonKey::OverwriteLangscore)] = writeObj.overwriteLangscore;
    write[key(JsonKey::OverwriteLangscoreCustom)] = writeObj.overwriteLangscoreCustom;
    write[key(JsonKey::EnableLanguagePatch)] = writeObj.enableLanguagePatch;
    write[key(JsonKey::EnableTranslationDefLang)] = writeObj.enableTranslateDefLang;
    write[key(JsonKey::WriteType)] = writeObj.writeMode;

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
            if(0 < numEmbededTypes) {
                textCategory.append(typesObj);
            }
        }
        if(textCategory.empty() == false) {
            script[key(JsonKey::ValidateTextCategory)] = textCategory;
        }

        basicDataList.append(script);
    }

    write[key(JsonKey::RPGMakerBasicData)] = basicDataList;
    write[key(JsonKey::IsFirstExported)] = this->isFirstExported;

    QJsonArray scripts;
    for(const auto& info : writeObj.scriptInfo)
    {
        QJsonObject script;
        script[key(JsonKey::Name)] = info.name;
        script[key(JsonKey::Ignore)] = info.ignore;
        QJsonArray ignorePointArray;
        for(auto& point : info.ignorePoint) {
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
    for(const auto& path : writeObj.ignorePicturePath) {
        ignorePictures.append(path);
    }
    write[key(JsonKey::IgnorePictures)] = ignorePictures;

    root[key(JsonKey::Write)] = write;

    root[key(JsonKey::PackingInputDir)] = this->packingInputDirectory;

    //ControlCharListへの書き込み
    QJsonObject validateJsonObjs;
    QJsonArray controlCharList;
    for(const auto& controlChar : this->validateObj.controlCharList) {
        controlCharList.append(controlChar);
    }
    validateJsonObjs[key(JsonKey::ControlCharList)] = controlCharList;

    root[key(JsonKey::Validate)] = validateJsonObjs;

    QJsonDocument doc(root);
    return doc.toJson();
}

void settings::write(QString path)
{
    QFile file(path);

    QFileInfo info(file);
    QDir parentDir = info.dir();
    if(parentDir.exists() == false) {
        parentDir.mkdir(parentDir.absolutePath());
    }

    if(file.open(QFile::WriteOnly)) {
        file.write(createJson());
    }
}

void settings::saveForProject() {
    write(this->langscoreProjectDirectory + "/config.json");
}

void settings::load(QString path)
{
    QFile file(path);
    if(file.open(QFile::ReadOnly) == false) { return; }

    if(projectType == ProjectType::None) {
        QFileInfo lsProjFile(path);
        auto parentDir = lsProjFile.absolutePath();
        parentDir.remove("_langscore");
        this->setGameProjectPath(parentDir);

        if(this->projectType == ProjectType::None) { return; }
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

    writeObj.exportDirectory = write[key(JsonKey::ExportDirectory)].toString("");
    writeObj.exportByLanguage = write[key(JsonKey::ExportByLang)].toBool(false);
    writeObj.enableLanguagePatch = write[key(JsonKey::EnableLanguagePatch)].toBool(false);
    writeObj.enableTranslateDefLang = write[key(JsonKey::EnableTranslationDefLang)].toBool(true);
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

        try {
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
        catch(const char* mes) {
            //std::cerr << mes << std::endl;
        }
    }

    auto jsonScripts = write[key(JsonKey::RPGMakerScripts)].toArray();
    auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath() + "/_list.csv");
    for(const auto& jsonInfo : jsonScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        auto fileName = jsonScript[key(JsonKey::Name)].toString();

        try
        {
            ScriptInfo& info = this->fetchScriptInfo(fileName);
            //スクリプト情報の名前がスクリプト名だった時の対応。数字列以外が含まれていたらスクリプト名とみなしてIDに変更する。
            if(std::count_if(fileName.cbegin(), fileName.cend(), [](const auto c) {
                return c < '0' || '9' < c;
                }) > 0) {
                auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&fileName](const auto& x) { return x[1] == fileName; });
                if(result != scriptList.cend()) {
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

                auto result = std::find_if(info.ignorePoint.begin(), info.ignorePoint.end(), [&pair](const auto& x) {
                    return x == std::pair<size_t, size_t>{pair.row, pair.col};
                    });
                if(result == info.ignorePoint.end()) {
                    info.ignorePoint.emplace_back(std::move(pair));
                }
            }
        }
        catch(const char* mes) {
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
    //QJsonArrayからcontrolCharListへの読み込み
    auto jsonControlCharList = validateJsonObj[key(JsonKey::ControlCharList)].toArray();
    for(const auto& jsonChar : jsonControlCharList) {
        this->validateObj.controlCharList.emplace_back(jsonChar.toString());
    }
}
