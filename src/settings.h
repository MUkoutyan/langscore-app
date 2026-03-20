#pragma once
#include <QStringList>
#include <QLocale>
#include <QMap>
#include <QFont>
#include <QCheckBox>
#include <vector>
#include "text_position.h"

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

    enum FontType {
        Global,
        Local
    };

    enum ValidateTextMode {
        Ignore,
        TextCount,
        TextWidth,
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
        int columnSize = 370;
        bool operator==(QString langName) const {
            return this->languageName == langName;
        }
    };

    struct ValidateTextInfo {
        ValidateTextMode mode = ValidateTextMode::Ignore;
        int width = 0;
        int count = 0;

        bool operator==(const ValidateTextInfo& infos) const noexcept {
            return !(this->operator!=(infos));
        }
        bool operator!=(const ValidateTextInfo& infos) const noexcept 
        {
            if(infos.mode != this->mode) { return true; }
            switch(this->mode) {
            case ValidateTextMode::Ignore:
                return false;
            case ValidateTextMode::TextCount:
                return infos.count != this->count;
            case ValidateTextMode::TextWidth:
                return infos.width != this->width;
            }
            return false;
        }

        int value() const noexcept {
            if(this->mode == ValidateTextMode::TextCount) {
                return this->count;
            }
            else if(this->mode == ValidateTextMode::TextWidth) {
                return this->width;
            }
            return 0;
        }
        void setValue(int v) noexcept {
            if(this->mode == ValidateTextMode::TextCount) {
                this->count = v;
            }
            else if(this->mode == ValidateTextMode::TextWidth) {
                this->width = v;
            }
        }
    };

    using TextValidationLangMap = std::map<QString, ValidateTextInfo>;
    using TextValidateTypeMap = std::map<QString, TextValidationLangMap>;

    struct ValidationProps
    {
        QStringList controlCharList;
    };


    //struct TextPoint
    //{
    //    size_t row = 0;
    //    size_t col = 0;
    //    QString argName;
    //    bool disable = false;	//元スクリプト変更によって位置が噛み合わなくなった場合true
    //    bool ignore = false;
    //    int writeMode = 0;
    //    bool operator==(const std::pair<size_t, size_t>& x) const noexcept {
    //        return row == x.first && col == x.second;
    //    }
    //    bool operator==(const QString& x) const noexcept {
    //        return x == argName;
    //    }
    //};

    //Write
    struct BasicData
    {
        QString fileName = "";       // analyzeフォルダにおけるファイル名
        QString filePath = "";       // analyzeフォルダにおけるファイルのパス(絶対パス)

        bool ignore = false;
        bool visible = true;
        int writeMode = 0;
        //key: typeName
        TextValidateTypeMap validateInfo = {};
    };

    struct MapInfo : public BasicData 
    {
        QString mapName = "";
        int mapID = -1;
        int order = 0;
        int parentMapID = 0;
    };

    struct ScriptInfo : public BasicData 
    {
        QString scriptName = "";
        std::vector<langscore::TextPosition> lines;

        bool isIgnore() const {
            return this->ignore || std::ranges::all_of(lines, [](const auto& pos) { return pos.ignore; });
        }
    };

    struct GraphInfo : public BasicData {
        QString extension;
        QString folder; //画像フォルダをルートとした際の相対パス。ファイル名は含まない。
        bool isFolder = false;
    };

    struct WriteProps
    {
        QString unisonCustomFuncComment = "";
        QString exportDirectory;
        bool exportByLanguage = false;
        bool overwriteLangscore = false;
        bool overwriteLangscoreCustom = false;
        bool enableTranslateDefLang = true;
        bool enableLanguagePatch = false;
        std::vector<BasicData> basicDataInfo;
        std::vector<MapInfo> mapDataInfo;
        std::vector<ScriptInfo> scriptInfo;
        std::vector<GraphInfo> graphDataInfo;

        std::vector<QString> ignorePicturePath;
        int writeMode = -1;
    };

    ProjectType projectType;
    QString gameProjectPath;
    std::vector<Language> languages;
    QString defaultLanguage;
    bool isFirstExported;
    bool isShowHiddenFilesOnTree;

    int originColumnWidth = 370;

    //Analyze
    QString langscoreProjectDirectory;
    QString transFileOutputDirName = "Translate";

    WriteProps writeObj;

    //FontType, fontIndex(from:addApplicationFont), FontFamily, fontFilePath
    std::vector<std::tuple<FontType, int, QStringList, QString>> fontIndexList;

    //Packing
    QString packingInputDirectory;
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
    MapInfo &fetchMapInfo(QString fileName);
    ScriptInfo &fetchScriptInfo(QString fileName);
    void removeScriptInfoPoint(QString fileName, langscore::TextPosition point);

    void setPackingDirectory(QString path);

    QByteArray createJson();
    void write(QString path);
    void saveForProject();
    void load(QString path);

    void updateLangscoreProjectPath();


    Font getDetafultFont() const;

    TextValidateTypeMap getValidationCsvData(QString fileName);
    TextValidateTypeMap& getValidationCsvDataRef(QString fileName);

    static QString getLowerBcp47Name(QLocale locale);
    static ProjectType getProjectType(const QString& path);

    static bool isLangscoreScript(const QString& scriptName);


    // Use settings types
    const std::vector<settings::ScriptInfo>& getScriptFileList() const { return writeObj.scriptInfo; }
    const std::vector<settings::BasicData>& getBasicInfoList() const { return writeObj.basicDataInfo; }
    const std::vector<settings::MapInfo>& getMapInfoList() const { return writeObj.mapDataInfo; }
    const std::vector<settings::GraphInfo>& getGraphicsFileList() const { return writeObj.graphDataInfo; }

    static QString scriptExt(settings::ProjectType type) {
        return type == settings::ProjectType::VXAce ? ".rb" : ".js";
    }

};

