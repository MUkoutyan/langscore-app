#include "settings.h"
#include "application_config.h"
#include "config_keys.h"
#include "csv.hpp"
#include "utility.hpp"

#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QApplication>
#include <QVersionNumber>

using namespace langscore;

inline const char* key(JsonKey key) {
    return configKey(key);
}

namespace {
    QJsonObject serializeLanguage(const settings::Language& l, const QString& projectDir);
    QJsonArray serializeLanguages(const std::vector<settings::Language>& languages, const QString& projectDir);
    QJsonObject serializeAnalyze(const settings* s);
    QJsonObject serializeWrite(const settings* s);
    QJsonArray serializeBasicDataList(const settings::settings::WriteProps& writeObj);
    QJsonArray serializeScripts(const settings::settings::WriteProps& writeObj);
    QJsonArray serializeIgnorePictures(const settings::settings::WriteProps& writeObj);
    QJsonObject serializeValidate(const settings::ValidationProps& validateObj);
    void deserializeLanguages(settings* s, const QJsonArray& jsonLanguages, const QString& configVer);
    void deserializeBasicDataList(settings* s, const QJsonArray& basicScripts);
    void deserializeMapInfoList(settings* s, const QJsonArray& basicScripts);
    void deserializeScripts(settings* s, const QJsonArray& jsonScripts, const QString& tempScriptFileDir);
    void deserializeIgnorePictures(settings* s, const QJsonArray& jsonIgnorePictures);
    void deserializeValidate(settings* s, const QJsonObject& validateJsonObj);

    void loadGraphicsFiles(settings* setting)
    {
        QDir graphicsFolder(setting->tempGraphicsFileDirectoryPath());
        auto& graphList = setting->writeObj.graphDataInfo;
        graphList.clear();

        // 再帰的にフォルダを走査し、ツリーを構築する関数
        std::function<void(const QDir&)> addGraphicsFolderToTree;
        addGraphicsFolderToTree = [setting, &addGraphicsFolderToTree, &graphicsFolder, &graphList](const QDir& dir)
        {
            // サブフォルダを取得
            QFileInfoList subFolders = dir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
            for(const auto& subFolder : subFolders)
            {
                QDir childDir(subFolder.absoluteFilePath());
                addGraphicsFolderToTree(childDir);
            }

            // 画像ファイルを取得
            QFileInfoList files = dir.entryInfoList(QStringList() << "*.jpg" << "*.png" << "*.bmp", QDir::Files);
            int ignorePictures = 0;
            const int numPictures = files.size();

            for(const auto& pict : files)
            {
                if(pict.size() == 0) { continue; }

                settings::GraphInfo info;
                info.fileName = pict.completeBaseName();
                info.extension = pict.suffix().toLower();
                info.filePath = pict.absoluteFilePath().replace('/', QDir::separator());

                QString relativeFolder = graphicsFolder.relativeFilePath(info.filePath);
                // ファイル名を除外してフォルダ部分のみ取得
                QFileInfo fi(relativeFolder);
                relativeFolder = fi.path();
                relativeFolder.replace('/', QDir::separator());
                info.folder = relativeFolder;

                auto& pixtureList = setting->writeObj.ignorePicturePath;
                auto result = std::find_if(pixtureList.begin(), pixtureList.end(), [&info](const auto& pic) {
                    return QFileInfo(pic).completeBaseName() == info.fileName;
                });
                if(result != pixtureList.end()) {
                    info.ignore = true;
                    ignorePictures++;
                }

                graphList.emplace_back(std::move(info));
            }
        };

        addGraphicsFolderToTree(graphicsFolder);
    }
} // namespace

QByteArray settings::createJson()
{
    QJsonObject root;
    root[key(JsonKey::ApplicationVersion)] = PROJECT_VER;
    root[key(JsonKey::ConfigVersion)] = CONFIG_FILE_VERSION;

    root[key(JsonKey::Languages)] = serializeLanguages(this->languages, this->langscoreProjectDirectory);
    root[key(JsonKey::DefaultLanguage)] = this->defaultLanguage;
    root[key(JsonKey::Project)] = "./" + QDir(this->langscoreProjectDirectory).relativeFilePath(this->gameProjectPath);
    root[key(JsonKey::Analyze)] = serializeAnalyze(this);
    root[key(JsonKey::Write)] = serializeWrite(this);
    root[key(JsonKey::PackingInputDir)] = this->packingInputDirectory;
    root[key(JsonKey::Validate)] = serializeValidate(this->validateObj);

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

    QJsonDocument doc = QJsonDocument::fromJson(QByteArray(file.readAll()));
    QJsonObject root = doc.object();

    auto projctVer = root[key(JsonKey::ApplicationVersion)].toString();
    auto configVer = root[key(JsonKey::ConfigVersion)].toString();

    this->languages.clear();
    deserializeLanguages(this, root[key(JsonKey::Languages)].toArray(), configVer);
    this->defaultLanguage = root[key(JsonKey::DefaultLanguage)].toString("ja");

    auto write = root[key(JsonKey::Write)].toObject();
    writeObj.exportDirectory = write[key(JsonKey::ExportDirectory)].toString("");
    writeObj.exportByLanguage = write[key(JsonKey::ExportByLang)].toBool(false);
    writeObj.enableLanguagePatch = write[key(JsonKey::EnableLanguagePatch)].toBool(false);
    writeObj.enableTranslateDefLang = write[key(JsonKey::EnableTranslationDefLang)].toBool(true);
    this->isFirstExported = write[key(JsonKey::IsFirstExported)].toBool();

    auto basicDataList = write[key(JsonKey::RPGMakerBasicData)].toArray();
    deserializeBasicDataList(this, basicDataList);
    deserializeMapInfoList(this, basicDataList);
    deserializeScripts(this, write[key(JsonKey::RPGMakerScripts)].toArray(), this->tempScriptFileDirectoryPath());
    deserializeIgnorePictures(this, write[key(JsonKey::IgnorePictures)].toArray());

    this->packingInputDirectory = root[key(JsonKey::PackingInputDir)].toString();
    deserializeValidate(this, root[key(JsonKey::Validate)].toObject());

    loadGraphicsFiles(this);
}

// --- Helper function definitions ---
namespace {

QJsonObject serializeLanguage(const settings::Language& l, const QString& projectDir) {
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
    } else {
        QDir baseDir2(projectDir);
        auto relativePath2 = baseDir2.relativeFilePath(l.font.filePath);
        langObj[key(JsonKey::FontPath)] = relativePath2;
    }
    return langObj;
}

QJsonArray serializeLanguages(const std::vector<settings::Language>& languages, const QString& projectDir) {
    QJsonArray langs;
    for(const auto& l : languages) {
        langs.append(serializeLanguage(l, projectDir));
    }
    return langs;
}

QJsonObject serializeAnalyze(const settings* s) {
    QJsonObject analyze;
    auto tempPath = QDir(s->langscoreProjectDirectory).relativeFilePath(s->analyzeDirectoryPath());
    analyze[key(JsonKey::TmpDir)] = "./" + tempPath;
    if(QFile::exists(s->langscoreProjectDirectory) == false) {
        QDir().mkdir(s->langscoreProjectDirectory);
    }
    return analyze;
}

QJsonObject serializeWrite(const settings* s) {
    QJsonObject write;
    write[key(JsonKey::UsCustomFuncComment)] = "Scripts/{0}#{1},{2}";
    write[key(JsonKey::ExportDirectory)] = QDir(s->langscoreProjectDirectory).relativeFilePath(s->writeObj.exportDirectory);
    write[key(JsonKey::ExportByLang)] = s->writeObj.exportByLanguage;
    write[key(JsonKey::OverwriteLangscore)] = s->writeObj.overwriteLangscore;
    write[key(JsonKey::OverwriteLangscoreCustom)] = s->writeObj.overwriteLangscoreCustom;
    write[key(JsonKey::EnableLanguagePatch)] = s->writeObj.enableLanguagePatch;
    write[key(JsonKey::EnableTranslationDefLang)] = s->writeObj.enableTranslateDefLang;
    write[key(JsonKey::WriteType)] = s->writeObj.writeMode;
    write[key(JsonKey::RPGMakerBasicData)] = serializeBasicDataList(s->writeObj);
    write[key(JsonKey::IsFirstExported)] = s->isFirstExported;
    write[key(JsonKey::RPGMakerScripts)] = serializeScripts(s->writeObj);
    write[key(JsonKey::IgnorePictures)] = serializeIgnorePictures(s->writeObj);
    return write;
}

QJsonArray serializeBasicDataList(const settings::settings::WriteProps& writeObj) 
{
    QJsonArray basicDataList;
    for(const auto& info : writeObj.basicDataInfo) 
    {
        QJsonObject script;
        script[key(JsonKey::Name)] = info.fileName;
        script[key(JsonKey::Ignore)] = info.ignore;

        QJsonArray textCategory;
        for(const auto& [typeName, lang_infos] : info.validateInfo) {
            QJsonObject typesObj;
            typesObj[key(JsonKey::Name)] = typeName;
            int numEmbededTypes = 0;
            for(const auto& [lang, info] : lang_infos) {
                if(info.mode == settings::ValidateTextMode::Ignore) { continue; }
                if(info.count == 0 && info.width == 0) { continue; }
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

    for(const auto& info : writeObj.mapDataInfo)
    {
        QJsonObject script;
        script[key(JsonKey::Name)] = info.fileName;
        script[key(JsonKey::Ignore)] = info.ignore;

        QJsonArray textCategory;
        for(const auto& [typeName, lang_infos] : info.validateInfo) {
            QJsonObject typesObj;
            typesObj[key(JsonKey::Name)] = typeName;
            int numEmbededTypes = 0;
            for(const auto& [lang, info] : lang_infos) {
                if(info.mode == settings::ValidateTextMode::Ignore) { continue; }
                if(info.count == 0 && info.width == 0) { continue; }
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

    return basicDataList;
}

QJsonArray serializeScripts(const settings::WriteProps& writeObj) 
{
    QJsonArray scripts;
    for(const auto& info : writeObj.scriptInfo) 
    {
        if(info.isIgnore() == false) { continue; }

        QJsonObject script;
        script[key(JsonKey::Name)] = info.fileName;
        script[key(JsonKey::Ignore)] = info.isIgnore();
        QJsonArray ignorePointArray;
        for(const auto& point : info.lines) 
        {
            if(point.ignore == false) { continue; }
            QJsonObject obj;

            if(std::holds_alternative<TextPosition::RowCol>(point.d))
            {
                auto& cell = std::get<TextPosition::RowCol>(point.d);
                obj[key(JsonKey::Row)] = qint64(cell.row);
                obj[key(JsonKey::Col)] = qint64(cell.col);
            }
            else 
            {
                auto& cell = std::get<TextPosition::ScriptArg>(point.d);
                obj[key(JsonKey::ParameterText)] = cell.valueName;
                
            }
            obj[key(JsonKey::Disable)] = false;
            script[key(JsonKey::Ignore)] = point.ignore;
            ignorePointArray.append(obj);
        }
        script[key(JsonKey::IgnorePoints)] = ignorePointArray;
        scripts.append(script);
    }
    return scripts;
}

QJsonArray serializeIgnorePictures(const settings::WriteProps& writeObj) {
    QJsonArray ignorePictures;
    for(const auto& path : writeObj.ignorePicturePath) {
        ignorePictures.append(path);
    }
    return ignorePictures;
}

QJsonObject serializeValidate(const settings::ValidationProps& validateObj) {
    QJsonObject validateJsonObjs;
    QJsonArray controlCharList;
    for(const auto& controlChar : validateObj.controlCharList) {
        controlCharList.append(controlChar);
    }
    validateJsonObjs[key(JsonKey::ControlCharList)] = controlCharList;
    return validateJsonObjs;
}

void deserializeLanguages(settings* s, const QJsonArray& jsonLanguages, const QString& configVer) 
{
    if(jsonLanguages.empty()) { return; }

    if(QVersionNumber::fromString(configVer) <= QVersionNumber::fromString("1.0.0")) 
    {
        for(auto l : jsonLanguages) {
            auto obj = l.toObject();
            QFont fontData(obj[key(JsonKey::FontName)].toString());
            fontData.setPixelSize(obj[key(JsonKey::FontSize)].toInt());
            auto data = settings::Language{
                obj[key(JsonKey::LanguageName)].toString(),
                settings::Font{
                    obj[key(JsonKey::FontName)].toString(),
                    obj[key(JsonKey::FontPath)].toString(),
                    fontData,
                    static_cast<uint32_t>(obj[key(JsonKey::FontSize)].toInt())
                },
                obj[key(JsonKey::Enable)].toBool(false)
            };
            s->languages.emplace_back(data);
        }
    } 
    else 
    {
        for(auto l : jsonLanguages) {
            auto obj = l.toObject();
            QFont fontData(obj[key(JsonKey::FontName)].toString());
            fontData.setPixelSize(obj[key(JsonKey::FontSize)].toInt());
            auto data = settings::Language{
                obj[key(JsonKey::LanguageName)].toString(),
                settings::Font{
                    obj[key(JsonKey::FontName)].toString(),
                    obj[key(JsonKey::FontPath)].toString(),
                    fontData,
                    static_cast<uint32_t>(obj[key(JsonKey::FontSize)].toInt())
                },
                obj[key(JsonKey::Enable)].toBool(false)
            };
            s->languages.emplace_back(data);
        }
    }
}

void deserializeBasicDataList(settings* s, const QJsonArray& basicScripts) 
{
    //ファイル情報のリストの作成
    {
        auto analyzeFolder = s->analyzeDirectoryPath();
        QDir dir{analyzeFolder};
        auto fileList = dir.entryInfoList({"*.json"}, QDir::Files);

        QRegularExpression re(R"(Map\d{3})");
        for(const auto& file : fileList) {
            auto fileViewName = file.completeBaseName();
            if(fileViewName == "Animations" || fileViewName == "Tilesets" || fileViewName == "MapInfos") { continue; }
            //Map~なければ追加。
            if(re.match(fileViewName).hasMatch() == false) {
                s->fetchBasicDataInfo(fileViewName);
            }
        }
    }

    for(auto jsonInfo : basicScripts) 
    {
        auto jsonScript = jsonInfo.toObject();
        auto name = jsonScript[key(JsonKey::Name)].toString();
        auto isIgnore = jsonScript[key(JsonKey::Ignore)].toBool();

        if(name.contains("Map")) { continue; }

        try {
            auto& info = s->fetchBasicDataInfo(name);
            info.ignore = isIgnore;
            if(jsonScript.contains(key(JsonKey::ValidateTextCategory)) && jsonScript[key(JsonKey::ValidateTextCategory)].isArray()) {
                auto typesArray = jsonScript[key(JsonKey::ValidateTextCategory)].toArray();
                for(const auto& valueTypesData : typesArray) {
                    if(valueTypesData.isObject() == false) { continue; }
                    auto valueTypes = valueTypesData.toObject();
                    auto keys = valueTypes.keys();
                    QString typeName = "";
                    settings::TextValidationLangMap langMap;
                    for(const auto& _key : keys) {
                        if(_key == key(JsonKey::Name)) {
                            typeName = valueTypes[_key].toString();
                        } else {
                            if(langMap.find(_key) == langMap.end()) {
                                langMap[_key] = {};
                            }
                            auto langValue = valueTypes[_key].toObject();
                            settings::ValidateTextInfo validateType;
                            validateType.mode = static_cast<settings::ValidateTextMode>(langValue[key(JsonKey::ValidateTextMode)].toInt());
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
        } catch(const char*) {
            // error handling
        }
    }
}

void deserializeMapInfoList(settings* s, const QJsonArray& basicScripts)
{
    auto analyzeFolder = s->analyzeDirectoryPath();

    // MapInfos.json をQtのJSON APIで読み込む
    QVector<QJsonObject> mapInfos;
    {
        QFile mapInfosFile(analyzeFolder + "/MapInfos.json");
        if(mapInfosFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray jsonData = mapInfosFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            if(doc.isObject())
            {
                //VXAceのMapInfos.jsonはルートがオブジェクト形式
                auto objs = doc.object();
                for(const auto& key : objs.keys()) {
                    QJsonValue v = objs[key];
                    if(v.isObject()) {
                        QJsonObject mapObj = v.toObject();
                        // キー（マップID）を数値に変換してidとして埋め込む
                        bool ok = false;
                        int mapId = key.toInt(&ok);
                        if(ok) {
                            mapObj.insert("id", mapId);
                        }
                        mapInfos.append(mapObj);
                    }
                }
            }
            else if(doc.isArray()) {
                //それ以外のMapInfos.jsonはルートが配列形式
                auto arr = doc.array();
                for(const auto& v : arr) {
                    qDebug() << v;
                    if(v.isObject()) {
                        mapInfos.append(v.toObject());
                    }
                }
            }
        }
    }

    //ファイル情報のリストの作成
    std::set<QString> files;
    {
        QDir dir{analyzeFolder};
        auto fileList = dir.entryInfoList({"*.json"}, QDir::Files);

        // さらに厳密に3桁数字かを正規表現でフィルタ
        QRegularExpression re(R"(Map\d{3})");
        for(const auto& file : fileList) {
            auto fileViewName = file.completeBaseName();
            if(re.match(fileViewName).hasMatch()) {
                files.insert(fileViewName);
                s->fetchMapInfo(fileViewName);
            }
        }
    }


    //ファイルの設定を復元
    for(auto jsonInfo : basicScripts)
    {
        auto jsonScript = jsonInfo.toObject();
        auto name = jsonScript[key(JsonKey::Name)].toString();
        auto isIgnore = jsonScript[key(JsonKey::Ignore)].toBool();

        if(name.contains("Map") == false) { continue; }
        if(name.contains("MapInfos")) { continue; }

        try {
            auto& info = s->fetchMapInfo(name);
            info.ignore = isIgnore;
            files.insert(::withoutAllExtension(name));
            if(jsonScript.contains(key(JsonKey::ValidateTextCategory)) && jsonScript[key(JsonKey::ValidateTextCategory)].isArray()) {
                auto typesArray = jsonScript[key(JsonKey::ValidateTextCategory)].toArray();
                for(const auto& valueTypesData : typesArray) {
                    if(valueTypesData.isObject() == false) { continue; }
                    auto valueTypes = valueTypesData.toObject();
                    auto keys = valueTypes.keys();
                    QString typeName = "";
                    settings::TextValidationLangMap langMap;
                    for(const auto& _key : keys) {
                        if(_key == key(JsonKey::Name)) {
                            typeName = valueTypes[_key].toString();
                        }
                        else {
                            if(langMap.find(_key) == langMap.end()) {
                                langMap[_key] = {};
                            }
                            auto langValue = valueTypes[_key].toObject();
                            settings::ValidateTextInfo validateType;
                            validateType.mode = static_cast<settings::ValidateTextMode>(langValue[key(JsonKey::ValidateTextMode)].toInt());
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
        catch(const char*) {
            // error handling
        }
    }

    if(mapInfos.isEmpty())
    {
        int dummyOrder = 0;
        for(const auto& file : files)
        {
            auto& info = s->fetchMapInfo(file);
            auto fileID = file;
            fileID.remove("Map");
            auto mapID = fileID.toInt();
            info.mapID = mapID;
            info.order = dummyOrder++;
        }
    }
    else {

        for(const auto& file : files)
        {
            auto fileID = file;
            fileID.remove("Map");
            auto mapID = fileID.toInt();

            // マップIDに対応するマップ情報を検索
            for(const auto& mapInfo : mapInfos)
            {
                // VXAceの場合、キーがマップIDとして使われている
                // MVやMZの場合は、マップ情報内にIDがある可能性がある
                bool found = false;
                int infoMapId = -1;

                if(mapInfo.contains("id")) {
                    infoMapId = mapInfo["id"].toInt();
                    found = (infoMapId == mapID);
                }
                else {
                    found = mapInfo.keys().contains(QString::number(mapID));
                }

                if(found == false && infoMapId != mapID) {
                    continue;
                }

                QString mapName;
                // キー一致または内部ID一致の場合、マップ情報を取得
                if(mapInfo.contains("@name")) {
                    mapName = mapInfo["@name"].toString();
                }
                else if(mapInfo.contains("name")) {
                    mapName = mapInfo["name"].toString();
                }

                int order = 0;
                if(mapInfo.contains("@order")) {
                    order = mapInfo["@order"].toInt();
                }
                else if(mapInfo.contains("order")) {
                    order = mapInfo["order"].toInt();
                }

                int parentID = 0;
                if(mapInfo.contains("@parent_id")) {
                    parentID = mapInfo["@parent_id"].toInt();
                }
                else if(mapInfo.contains("parentId")) {
                    parentID = mapInfo["parentId"].toInt();
                }

                auto& info = s->fetchMapInfo(file);
                info.mapID = mapID;
                info.order = order;
                info.parentMapID = parentID;
                info.mapName = mapName;

                break;
            }
        }
    }
}

void deserializeScripts(settings* s, const QJsonArray& jsonScripts, const QString& tempScriptFileDir) 
{

    auto analyzeFolder = s->analyzeDirectoryPath();
    auto scriptFolder = s->tempScriptFileDirectoryPath();

    QFile jsonFile(analyzeFolder + "/Scripts.lsjson");
    if(!jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open JSON file:" << jsonFile.fileName();
        return;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << jsonFile.fileName();
        return;
    }

    QJsonArray jsonArray = jsonDoc.array();


    auto scriptExt = langscore::GetScriptExtension(s->projectType);
    for(const QJsonValue& value : jsonArray)
    {
        QJsonObject obj = value.toObject();
        QString fileName = obj["file"].toString();

        TextPosition line;
        line.value = obj["original"].toString();

        // TextPositionの生成
        //pos.scriptFileName = fileName;
        if(obj.contains("parameterName")) {
            line.type = TextPosition::Type::Argument;
            if(obj.contains("value")) {
                line.value = obj["value"].toString();
            }
            TextPosition::ScriptArg rowCol;
            rowCol.valueName = obj["parameterName"].toString();
            line.d = rowCol;
        }
        else {
            line.type = TextPosition::Type::RowCol;
            if(obj.contains("value")) {
                line.value = obj["value"].toString();
            }
            TextPosition::RowCol rowCol;
            rowCol.row = obj["row"].toInteger();
            rowCol.col = obj["col"].toInteger();
            line.d = rowCol;
        }

        // ScriptInfoへ追加
        auto& info = s->fetchScriptInfo(fileName);
        info.lines.push_back(std::move(line));
    }

    // config.json から ScriptInfo を復元
    for(const auto& jsonInfo : jsonScripts) {
        auto jsonScript = jsonInfo.toObject();
        auto fileName = jsonScript[key(JsonKey::Name)].toString();
        try {
            auto& info = s->fetchScriptInfo(fileName);
            info.ignore = jsonScript[key(JsonKey::Ignore)].toBool(false);
            info.lines.clear();

            if(jsonScript.contains(key(JsonKey::IgnorePoints)) && jsonScript[key(JsonKey::IgnorePoints)].isArray()) {
                auto ignorePointArray = jsonScript[key(JsonKey::IgnorePoints)].toArray();
                for(const auto& jsonPoint : ignorePointArray) {
                    auto obj = jsonPoint.toObject();
                    langscore::TextPosition pos;

                    // パラメータ名がある場合は ScriptArg
                    if(obj.contains(key(JsonKey::ParameterText))) {
                        pos.type = langscore::TextPosition::Type::Argument;
                        langscore::TextPosition::ScriptArg arg;
                        arg.valueName = obj[key(JsonKey::ParameterText)].toString();
                        pos.d = arg;
                    }
                    else {
                        // RowCol
                        pos.type = langscore::TextPosition::Type::RowCol;
                        langscore::TextPosition::RowCol rc;
                        rc.row = obj[key(JsonKey::Row)].toInteger();
                        rc.col = obj[key(JsonKey::Col)].toInteger();
                        pos.d = rc;
                    }
                    // ignore, disable などのフラグ
                    pos.ignore = obj[key(JsonKey::Ignore)].toBool(false);
                    // 他に必要な値があればここでセット
                    info.lines.push_back(std::move(pos));
                }
            }
        }
        catch(const char*) {
            // error handling
        }
    }

    if(s->projectType == settings::VXAce)
    {
        auto scriptPairs = langscore::readCsv(scriptFolder + "/_list.csv");
        auto& scriptList = s->writeObj.scriptInfo;
        for(auto& scriptFile : scriptList)
        {
            for(auto& pairs : scriptPairs)
            {
                if(pairs.size() < 2) { continue; } // スクリプト名とファイル名が必要
                QString fileName = pairs[0];
                QString scriptName = pairs[1];
                if(withoutAllExtension(scriptFile.fileName) == fileName) {
                    scriptFile.scriptName = scriptName;
                }
            }
        }
    }
    else 
    {
        auto& scriptList = s->writeObj.scriptInfo;
        for(auto& scriptFile : scriptList) {
            scriptFile.scriptName = ::withoutAllExtension(scriptFile.fileName);
        }
    }



    //auto scriptList = langscore::readCsv(tempScriptFileDir + "/_list.csv");
    //for(const auto& jsonInfo : jsonScripts) {
    //    auto jsonScript = jsonInfo.toObject();
    //    auto fileName = jsonScript[key(JsonKey::Name)].toString();
    //    try {
    //        auto& info = s->fetchScriptInfo(fileName);
    //        if(std::count_if(fileName.cbegin(), fileName.cend(), [](const auto c) {
    //            return c < '0' || '9' < c;
    //           }) > 0
    //        ){
    //        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&fileName](const auto& x) { return x[1] == fileName; });
    //            if(result != scriptList.cend()) {
    //                info.fileName = (*result)[0];
    //            }
    //        }
    //        info.ignore = jsonScript[key(JsonKey::Ignore)].toBool();
    //        auto jsonIgnorePoints = jsonScript[key(JsonKey::IgnorePoints)].toArray();
    //        for(const auto& jsonPoint : jsonIgnorePoints) {
    //            auto obj = jsonPoint.toObject();
    //            auto pair = settings::ScriptInfo{
    //                static_cast<size_t>(obj[key(JsonKey::Row)].toInteger()),
    //                static_cast<size_t>(obj[key(JsonKey::Col)].toInteger()),
    //                obj[key(JsonKey::Name)].toString(),
    //                obj[key(JsonKey::Disable)].toBool(false),
    //                obj[key(JsonKey::Ignore)].toBool(false),
    //                obj[key(JsonKey::WriteType)].toInt()
    //            };
    //            auto result = std::find_if(info.ignorePoint.begin(), info.ignorePoint.end(), [&pair](const auto& x) {
    //                return x == std::pair<size_t, size_t>{pair.row, pair.col};
    //            });
    //            if(result == info.ignorePoint.end()) {
    //                info.ignorePoint.emplace_back(std::move(pair));
    //            }
    //        }
    //    } catch(const char*) {
    //        // error handling
    //    }
    //}
}

void deserializeIgnorePictures(settings* s, const QJsonArray& jsonIgnorePictures) {
    for(auto jsonPath : jsonIgnorePictures) {
        s->writeObj.ignorePicturePath.emplace_back(jsonPath.toString());
    }
}

void deserializeValidate(settings* s, const QJsonObject& validateJsonObj) {
    auto jsonControlCharList = validateJsonObj[key(JsonKey::ControlCharList)].toArray();
    for(const auto& jsonChar : jsonControlCharList) {
        s->validateObj.controlCharList.emplace_back(jsonChar.toString());
    }
}

} // namespace
