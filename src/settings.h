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
        bool enable;
        bool operator==(QString langName) const {
            return this->languageName == langName;
        }
    };

    ProjectType projectType;
    QString gameProjectPath;
    std::vector<Language> languages;
    QString defaultLanguage;

    //Analyze
    QString langscoreProjectDirectory;
    QString transFileOutputDirName = "Translate";

    std::vector<int> fontIndexList;

    //Write
    struct BasicData
    {
        QString name = "";
        bool ignore = false;
        int writeMode = 0;
    };

    struct TextPoint
    {
        size_t row = 0;
        size_t col = 0;
        bool disable = false;	//元スクリプト変更によって位置が噛み合わなくなった場合true
        bool ignore = false;
        int writeMode = 0;
        bool operator==(const std::pair<size_t,size_t>& x) const noexcept {
            return row == x.first && col == x.second;
        }
    };
    struct ScriptInfo : public BasicData
    {
        std::vector<TextPoint> ignorePoint;
    };

    struct WriteProps
    {
        QString unisonCustomFuncComment = "";
        QString exportDirectory;
        bool exportByLanguage = false;
        std::vector<ScriptInfo> scriptInfo;
        std::vector<BasicData> basicDataInfo;

        std::vector<QString> ignorePicturePath;
    };
    WriteProps writeObj;

    QString translateDirectoryPath() const;
    QString tempFileDirectoryPath() const;
    QString tempScriptFileDirectoryPath() const;
    QString tempGraphicsFileDirectoryPath() const;

    Language& fetchLanguage(QString bcp47Name);
    void removeLanguage(QString bcp47Name);

    ScriptInfo &fetchScriptInfo(QString fileName);
    void removeScriptInfoPoint(QString fileName, std::pair<size_t, size_t> point);

    QByteArray createJson();
    void write(QString path);
    void saveForProject();
    void load(QString path);

};

