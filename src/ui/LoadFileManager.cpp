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

void LoadFileManager::loadFile(std::shared_ptr<settings> setting)
{
    auto analyzeFolder = setting->analyzeDirectoryPath();
    // MapInfos.json をQtのJSON APIで読み込む
    QVector<QJsonObject> mapInfos;
    {
        QFile mapInfosFile(analyzeFolder + "/MapInfos.json");
        if (mapInfosFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QByteArray jsonData = mapInfosFile.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(jsonData);
            if (doc.isArray()) {
                QJsonArray arr = doc.array();
                for (const QJsonValue& v : arr) {
                    if (v.isObject()) {
                        mapInfos.append(v.toObject());
                    } else {
                        mapInfos.append(QJsonObject()); // nullや非objectは空objectで埋める
                    }
                }
            }
        }
    }

    QDir sourceDir(analyzeFolder);
    auto files = sourceDir.entryInfoList();

    for(const auto& file : files)
    {
        if(file.suffix() != "json") { continue; }

        auto fileViewName = file.completeBaseName();
        if(fileViewName == "config") { continue; }
        else if(fileViewName == "Scripts") { continue; }
        else if(fileViewName == "Graphics") { continue; }

        if(QFile::exists(analyzeFolder + "/" + fileViewName + ".lsjson") == false) {
            continue;
        }
        int mapID = -1;
        QString mapName;
        if(QRegularExpression("Map\\d\\d\\d").match(fileViewName).hasMatch())
        {
            auto fileID = fileViewName;
            fileID.remove("Map");
            bool isOK = false;
            auto mapID = fileID.toInt(&isOK);
            if(isOK) 
            {
                // mapInfos から該当mapIDのnameを探す
                for(const QJsonObject& obj : mapInfos) {
                    if(!obj.isEmpty() && obj.contains("id") && obj["id"].toInt() == mapID) {
                        mapName = obj.value("name").toString();
                        break;
                    }
                }
            }
        }

        this->basicFileList.emplace_back(FileInfo{
            fileViewName,
            file.absoluteFilePath(),
            mapName,
            Qt::Checked
        });

    }


    auto scriptFolder = setting->tempScriptFileDirectoryPath();
    if(setting->projectType == settings::VXAce) {
        auto scriptFiles = langscore::readCsv(scriptFolder + "/_list.csv");
        for(auto & scriptFile : scriptFiles) {
            if(scriptFile.size() < 2) { continue; } // スクリプト名とファイル名が必要
            QString fileName = scriptFile[0];
            QString scriptName = scriptFile[1];
            ScriptInfo info = {
                fileName, scriptFolder + "/" + fileName,
                "", Qt::Checked,
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
        
        std::vector<QString> result;
        for(const QJsonValue& value : jsonArray) 
        {
            QJsonObject obj = value.toObject();
            QString file = obj["file"].toString();
            if(file.isEmpty() == false) {
                ScriptInfo info = {
                    file, scriptFolder + "/" + file + "." + scriptExt,
                    "", Qt::Checked,
                    file
                };
                this->scriptFileList.emplace_back(std::move(info));
            }
        }
    }

    {
        QDir graphicsFolder(setting->tempGraphicsFileDirectoryPath());
        QFileInfoList graphFolders = graphicsFolder.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
        for(const auto& graphFolder : graphFolders)
        {
            QDir childFolder(graphFolder.absoluteFilePath());
            QFileInfoList files = childFolder.entryInfoList(QStringList() << "*.*", QDir::Files);

            for(const auto& pict : files) 
            {
                GraphInfo info = {
                    pict.completeBaseName(), pict.absoluteFilePath(),
                    "", Qt::Checked, {}
                };
                info.folder = childFolder.relativeFilePath(pict.absoluteFilePath());
                this->graphicsFileList.emplace_back(std::move(info));
            }
        }
    }

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
