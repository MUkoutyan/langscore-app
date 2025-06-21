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

void LoadFileManager::loadFile(std::shared_ptr<settings> setting)
{
    this->loadBasicFiles(setting);
    this->loadScriptFiles(setting);
    this->loadGraphicsFiles(setting);
}

QString LoadFileManager::getScriptName(QString fileName)
{
    fileName = QFileInfo(fileName).completeBaseName();
    auto result = std::find_if(scriptFileList.cbegin(), scriptFileList.cend(), [fileName](const auto& x) {
        return x.fileName == fileName;
        });
    if(result == scriptFileList.cend()) {
        return "";
    }
    return result->scriptName;
}

QString LoadFileManager::getScriptFileName(QString scriptName)
{
    auto result = std::find_if(scriptFileList.cbegin(), scriptFileList.cend(), [scriptName](const auto& x) {
        return x.scriptName == scriptName;
        });
    if(result == scriptFileList.cend()) {
        return "";
    }
    return result->fileName;
}

void LoadFileManager::setCheckState(const QString& fileName, Qt::CheckState state)
{
    auto it = std::find_if(basicFileList.begin(), basicFileList.end(), [&fileName](const FileInfo& file) {
        return file.fileName == fileName;
    });
    if(it != basicFileList.end()) {
        it->isEnableState = state;
        return;
    }

    auto itScript = std::find_if(scriptFileList.begin(), scriptFileList.end(), [&fileName](const ScriptInfo& script) {
        return script.fileName == fileName;
    });
    if(itScript != scriptFileList.end()) {
        itScript->isEnableState = state;
        return;
    }
}

void LoadFileManager::loadBasicFiles(std::shared_ptr<settings> setting)
{
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

            MapInfo info = 
            {
                fileViewName,
                file.absoluteFilePath(),
                Qt::Checked,
                mapID,
                dummyOrder++,
                0,
                ""
            };
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
                MapInfo info = {
                    fileViewName,
                    file.absoluteFilePath(),
                    Qt::Checked,
                    mapID,
                    order,
                    parentID,
                    mapName
                };
                this->basicFileList.emplace_back(std::move(info));
                break;
            }
        }
    }

}

void LoadFileManager::loadScriptFiles(std::shared_ptr<settings> setting)
{
    auto analyzeFolder = setting->analyzeDirectoryPath();
    auto scriptFolder = setting->tempScriptFileDirectoryPath();

    if(setting->projectType == settings::VXAce) 
    {
        auto scriptFiles = langscore::readCsv(scriptFolder + "/_list.csv");
        for(auto& scriptFile : scriptFiles) {
            if(scriptFile.size() < 2) { continue; } // スクリプト名とファイル名が必要
            QString fileName = scriptFile[0];
            QString scriptName = scriptFile[1];
            ScriptInfo info = {
                fileName, scriptFolder + "/" + fileName,
                Qt::Checked,
                scriptName
            };
            this->scriptFileList.emplace_back(std::move(info));
        }
    }
    else if(setting->projectType == settings::MV || setting->projectType == settings::MZ)
    {

        QFile jsonFile(analyzeFolder + "/Scripts.lsjson");

        if(false == jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
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

        auto scriptExt = langscore::GetScriptExtension(setting->projectType);
        QJsonArray jsonArray = jsonDoc.array();

        std::set<QString> loadedScriptFileName;
        std::vector<QString> result;
        for(const QJsonValue& value : jsonArray)
        {
            QJsonObject obj = value.toObject();
            QString file = obj["file"].toString();
            if(file.isEmpty()) { continue; }

            if(loadedScriptFileName.contains(file)) { continue; } // 重複を避ける

            ScriptInfo info = {
                file, scriptFolder + "/" + file + "." + scriptExt,
                Qt::Checked,
                file
            };
            this->scriptFileList.emplace_back(std::move(info));

            loadedScriptFileName.insert(file);
        }
    }

}

void LoadFileManager::loadGraphicsFiles(std::shared_ptr<settings> setting)
{
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

            GraphInfo info = {
                pict.completeBaseName(), pict.absoluteFilePath(),
                Qt::Checked, {}
            };

            // graphicsFolderをルートとした相対パス（ファイル名は含まない）を格納
            QString relativeFolder = graphicsFolder.relativeFilePath(childFolder.absolutePath());
            info.folder = relativeFolder;
            this->graphicsFileList.emplace_back(std::move(info));
        }
    }
}
