#pragma once

#include <vector>
#include <QString>
#include "../settings.h"

class LoadFileManager {
public:
    LoadFileManager() = default;
    ~LoadFileManager() = default;

    struct FileInfo {
        QString fileName;       //analyzeフォルダにおけるファイル名
        QString filePath;       //analyzeフォルダにおけるファイルのパス(絶対パス)
        Qt::CheckState isEnableState = Qt::Checked;
    };
    struct MapInfo : public FileInfo {
        int mapID = -1;
        int order = 0;
        int parentMapID = 0;
        QString mapName;
    };
    struct ScriptInfo : public FileInfo {
        QString scriptName;
    };
    struct GraphInfo : public FileInfo {
        QString folder; //画像フォルダをルートとした際の相対パス
    };

    void loadFile(std::shared_ptr<settings> setting);

    QString getScriptName(QString fileName);
    QString getScriptFileName(QString scriptName);

    const std::vector<ScriptInfo>& getScriptFileList() const { return scriptFileList; }
    const std::vector<MapInfo>& getBasicFileList() const { return basicFileList; }
    const std::vector<GraphInfo>& getGraphicsFileList() const { return graphicsFileList; }

    void setCheckState(const QString& fileName, Qt::CheckState state);


private:
    std::vector<MapInfo> basicFileList;
    std::vector<ScriptInfo> scriptFileList;
    std::vector<GraphInfo> graphicsFileList;

    void loadBasicFiles(std::shared_ptr<settings> setting);
    void loadScriptFiles(std::shared_ptr<settings> setting);
    void loadGraphicsFiles(std::shared_ptr<settings> setting);

};