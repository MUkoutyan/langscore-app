#pragma once
#include <QStringList>
#include <QMap>

class settings
{
public:
    settings();

    //Common
    QString project;
    QStringList languages;
    QString defaultLanguage;

    //Analuze
    QString outputDirectory;

    //Write
    struct WriteProps
    {
        QString transFileOutputDirName = "Translate";
        QString unisonCustomFuncComment = "";
        struct Font {
            QString name = "";
            std::uint32_t size = 22;
        };
        QMap<QString, Font> langFont;

        struct ScriptInfo{
            QString name  = "";
            size_t line   = 0;
            size_t col    = 0;
            int writeMode = 0;
            bool ignore   = false;
        };
        std::vector<ScriptInfo> ignoreScriptInfo;
    };
    QMap<QString, WriteProps> write;

};

