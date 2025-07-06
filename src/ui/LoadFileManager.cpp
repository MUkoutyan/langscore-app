#include "LoadFileManager.h"
#include <QFileInfo>
#include "../utility.hpp"
#include "../csv.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDir>
#include <ranges>

#include <QDebug>

using namespace langscore;

void LoadFileManager::loadFile(std::shared_ptr<settings> setting)
{
    //this->loadBasicFiles(setting);
    //this->loadScriptFiles(setting);
    //this->loadGraphicsFiles(setting);
}

void LoadFileManager::loadBasicFiles(std::shared_ptr<settings> setting)
{
    // 参照でsettingsのMapInfoリストを取得し、クリアしてから再構築
    auto& mapList = setting->writeObj.basicDataInfo;
    mapList.clear();

    auto analyzeFolder = setting->analyzeDirectoryPath();

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

    QDir sourceDir(analyzeFolder);
    auto files = sourceDir.entryInfoList();

    files.removeIf([&](const QFileInfo& mapFile) 
    {
        if(mapFile.suffix() != "json") { return true; }

        auto fileViewName = mapFile.completeBaseName();
        if(fileViewName == "config") { return true; }
        else if(fileViewName == "Scripts") { return true; }
        else if(fileViewName == "Graphics") { return true; }

        if(QFile::exists(analyzeFolder + "/" + fileViewName + ".lsjson") == false) {
            return true;
        }
        if(QRegularExpression("Map\\d\\d\\d").match(fileViewName).hasMatch() == false) {
            return true;
        }        
        
        auto fileID = fileViewName;
        fileID.remove("Map");
        bool isOK = false;
        auto mapID = fileID.toInt(&isOK);
        return isOK == false;
    });

    if(mapInfos.isEmpty())
    {
        int dummyOrder = 0;
        for(const auto& file : files)
        {
            auto fileViewName = file.completeBaseName();
            auto fileID = fileViewName;
            fileID.remove("Map");
            auto mapID = fileID.toInt();

            settings::MapInfo info;
            info.fileName = fileViewName;
            info.filePath = file.absoluteFilePath();
            info.mapID = mapID;
            info.order = dummyOrder++;
            mapList.emplace_back(std::move(info));
        }
    }
    else {

        for(const auto& file : files)
        {
            auto fileViewName = file.completeBaseName();
            auto fileID = fileViewName;
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

                // 見つかったら追加の処理を行ってループを抜ける
                settings::MapInfo info;
                info.fileName = fileViewName;
                info.filePath = file.absoluteFilePath();
                info.mapID = mapID;
                info.order = order;
                info.parentMapID = parentID;
                info.mapName = mapName;

                mapList.emplace_back(std::move(info));
                break;
            }
        }
    }
}

void LoadFileManager::loadScriptFiles(std::shared_ptr<settings> setting)
{
    // 参照でsettingsのScriptInfoリストを取得し、クリアしてから再構築
    auto& scriptList = setting->writeObj.scriptInfo;
    scriptList.clear();

    auto analyzeFolder = setting->analyzeDirectoryPath();
    auto scriptFolder = setting->tempScriptFileDirectoryPath();

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

    // ファイル名ごとにScriptInfoを作成
    QHash<QString, settings::ScriptInfo> fileNameToScriptInfo;

    auto scriptExt = langscore::GetScriptExtension(setting->projectType);
    for(const QJsonValue& value : jsonArray) 
    {
        QJsonObject obj = value.toObject();
        QString fileName = obj["file"].toString();

        langscore::TextPosition line;
        line.value = obj["original"].toString();

        // TextPositionの生成
        TextPosition pos;
        if(obj.contains("parameterName")) {
            pos.type = TextPosition::Type::Argument;
            if(obj.contains("value")) {
                pos.value = obj["value"].toString();
            }
            TextPosition::ScriptArg rowCol;
            rowCol.valueName = obj["parameterName"].toString();
            pos.d = rowCol;
        }
        else {
            pos.type = TextPosition::Type::RowCol;
            if(obj.contains("value")) {
                pos.value = obj["value"].toString();
            }
            TextPosition::RowCol rowCol;
            rowCol.row = obj["row"].toInteger();
            rowCol.col = obj["col"].toInteger();
            pos.d = rowCol;
        }

        // ScriptInfoへ追加
        auto& info = fileNameToScriptInfo[fileName];
        info.fileName = fileName;
        info.filePath = scriptFolder + "/" + fileName + "." + scriptExt;
        info.lines.push_back(std::move(line));
    }

    // scriptNameの付与
    for(auto it = fileNameToScriptInfo.begin(); it != fileNameToScriptInfo.end(); ++it) {
        scriptList.emplace_back(std::move(it.value()));
    }

    if(setting->projectType == settings::VXAce) 
    {
        auto scriptPairs = langscore::readCsv(scriptFolder + "/_list.csv");
        for(auto& scriptFile : scriptList) 
        {
            for(auto& pairs : scriptPairs)
            {
                if(pairs.size() < 2) { continue; } // スクリプト名とファイル名が必要
                QString fileName = pairs[0];
                QString scriptName = pairs[1];
                if(scriptFile.fileName == fileName) {
                    scriptFile.scriptName = scriptName;
                }
            }
        }
    }
    else {
        for(auto& scriptFile : scriptList) {
            scriptFile.scriptName = scriptFile.fileName;
        }
    }
}

void LoadFileManager::loadGraphicsFiles(std::shared_ptr<settings> setting)
{
    // 参照でsettingsのGraphInfoリストを取得し、クリアしてから再構築
    auto& graphList = setting->writeObj.graphDataInfo;
    graphList.clear();

    QDir graphicsFolder(setting->tempGraphicsFileDirectoryPath());
    QFileInfoList graphFolders = graphicsFolder.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(const auto& graphFolder : graphFolders)
    {
        QDir childFolder(graphFolder.absoluteFilePath());
        QFileInfoList files = childFolder.entryInfoList(QStringList() << "*.*", QDir::Files);

        for(const auto& pict : files)
        {
            //画像ファイルかどうかの判定
            static const QStringList imageExtensions = {
                "png", "jpg", "jpeg", "bmp", "gif", "tiff", "webp"
            };
            if (!imageExtensions.contains(pict.suffix().toLower())) {
                continue;
            }

            settings::GraphInfo info;
            info.fileName = pict.completeBaseName();
            info.filePath = pict.absoluteFilePath();

            // graphicsFolderをルートとした相対パス（ファイル名は含まない）を格納
            QString relativeFolder = graphicsFolder.relativeFilePath(childFolder.absolutePath());
            info.folder = relativeFolder;
            graphList.emplace_back(std::move(info));
        }
    }
}
