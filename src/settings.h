#pragma once
#include <QStringList>
#include <QMap>

class settings
{
public:
    settings();

    //Common
    enum ProjectType{
        VXAce,
        MV,
        MZ
    };

    struct Font {
        QString name = "";
        std::uint32_t size = 22;
    };
    struct Language{
        QString languageName;
        Font font;
        bool operator==(QString langName) const {
            return this->languageName == langName;
        }
    };

    ProjectType projectType;
    QString gameProjectPath;
    std::vector<Language> languages;
    QString defaultLanguage;

    //Analyze
    QString tempFileOutputDirectory;
    QString transFileOutputDirName = "Translate";

    std::vector<int> fontIndexList;

    //Write
    struct WriteProps
    {
        QString unisonCustomFuncComment = "";

        struct ScriptInfo{
            QString name  = "";
            std::vector<std::pair<size_t, size_t>> ignorePoint;
            int writeMode = 0;
            bool ignore   = false;
        };
        std::vector<ScriptInfo> ignoreScriptInfo;

        std::vector<QString> ignorePicturePath;
    };
    WriteProps writeObj;

    QString translateDirectoryPath() const;
    QString tempFileDirectoryPath() const;
    QString tempScriptFileDirectoryPath() const;
    QString tempGraphicsFileDirectoryPath() const;

    Language& fetchLanguage(QString bcp47Name);
    void removeLanguage(QString bcp47Name);

    QByteArray createJson();
    void write(QString path);
    void saveForProject();
    void load(QString path);

};

