#pragma once
#include <QStringList>
#include <QMap>

class settings
{
public:
    settings();

    //Common
    QString gameProjectPath;
    QStringList languages;
    QString defaultLanguage;

    //Analyze
    QString tempFileOutputDirectory;
    QString transFileOutputDirName = "Translate";

    //Write
    struct WriteProps
    {
        QString unisonCustomFuncComment = "";
        struct Font {
            QString name = "";
            std::uint32_t size = 22;
        };
        QMap<QString, Font> langFont;

        struct ScriptInfo{
            QString name  = "";
            std::vector<std::pair<size_t, size_t>> ignorePoint;
            int writeMode = 0;
            bool ignore   = false;
        };
        std::vector<ScriptInfo> ignoreScriptInfo;
    };
    WriteProps writeObj;

    QString translateDirectoryPath() const;
    QString tempFileDirectoryPath() const;
    QString tempScriptFileDirectoryPath() const;

    void write(QString path);
    void save();

};

