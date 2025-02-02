#pragma once
#include <QStringList>
#include <QLocale>
#include <QMap>
#include <QFont>

class settings
{
public:
    settings();
    void setGameProjectPath(QString absolutePath);

    //Common
    enum ProjectType{
        None,
        VXAce,
        MV,
        MZ
    };

    struct Font {
        QString name = "";
        QString filePath = "";
        QFont fontData;
        std::uint32_t size = 0;
    };
    struct Language{
        QString languageName = "";
        Font font;
        bool enable = false;
        bool operator==(QString langName) const {
            return this->languageName == langName;
        }
    };

    ProjectType projectType;
    QString gameProjectPath;
    std::vector<Language> languages;
    QString defaultLanguage;
    bool isFirstExported;

    //Analyze
    QString langscoreProjectDirectory;
    QString transFileOutputDirName = "Translate";

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
        QString argName;
        bool disable = false;	//元スクリプト変更によって位置が噛み合わなくなった場合true
        bool ignore = false;
        int writeMode = 0;
        bool operator==(const std::pair<size_t,size_t>& x) const noexcept {
            return row == x.first && col == x.second;
        }
        bool operator==(const QString& x) const noexcept {
            return x == argName;
        }
    };
    struct ScriptInfo : public BasicData
    {
        std::vector<TextPoint> ignorePoint;
    };

    enum FontType {
        Global,
        Local
    };

    struct WriteProps
    {
        QString unisonCustomFuncComment = "";
        QString exportDirectory;
        bool exportByLanguage = false;
        bool overwriteLangscore = false;
        bool overwriteLangscoreCustom = false;
        bool enableLanguagePatch = false;
        std::vector<ScriptInfo> scriptInfo;
        std::vector<BasicData> basicDataInfo;

        std::vector<QString> ignorePicturePath;
        int writeMode = -1;
    };
    WriteProps writeObj;

    //FontType, fontIndex(from:addApplicationFont), FontFamily, fontFilePath
    std::vector<std::tuple<FontType, int, QStringList, QString>> fontIndexList;

    //Packing
    QString packingInputDirectory;

    enum ValidateTextMode {
        Ignore,
        TextCount,
        TextWidth,
    };

    struct ValidationProps
    {
        struct CSVData {
            QString name = "";  //PackingInputDir + nameでパスになる
            ValidateTextMode mode = ValidateTextMode::Ignore;
            int validationNumber = 0;
        };
        //全体共通の値
        ValidateTextMode mode = ValidateTextMode::Ignore;
        int validationNumber = 0;
        std::vector<CSVData> csvDataList;
    };
    ValidationProps validateObj;

    void setupLanguages(const std::vector<QLocale> &locales);

    void setupFontList();
    std::vector<Font> createFontList() const;
    QString getDefaultFontName() const;
    std::uint32_t getDefaultFontSize() const;

    QString translateDirectoryPath() const;
    QString analyzeDirectoryPath() const;
    QString tempScriptFileDirectoryPath() const;
    QString tempGraphicsFileDirectoryPath() const;

    bool isWritedLangscorePlugin() const;
    bool isWritedLangscoreCustomPlugin() const;

    Language& fetchLanguage(QString bcp47Name);
    void removeLanguage(QString bcp47Name);

    BasicData &fetchBasicDataInfo(QString fileName);
    ScriptInfo &fetchScriptInfo(QString fileName);
    void removeScriptInfoPoint(QString fileName, std::pair<size_t, size_t> point);

    static QString getLowerBcp47Name(QLocale locale);

    void setPackingDirectory(QString path);

    QByteArray createJson();
    void write(QString path);
    void saveForProject();
    void load(QString path);

    void updateLangscoreProjectPath();

    static ProjectType getProjectType(const QString& path);

    Font getDetafultFont() const;

};

