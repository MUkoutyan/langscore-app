#include "settings.h"
#include "utility.hpp"
#include <QDir>
#include <QFile>
#include <QFontDatabase>
#include <QApplication>
#include <assert.h>
#include "csv.hpp"

using namespace langscore;

namespace {

const static QMap<QString, settings::ProjectType> projectExtensionAndType = {
    {"rvproj2",     settings::VXAce},
    {"rpgproject",  settings::MV},
    {"rmmzproject", settings::MZ},
};

}

settings::settings()
    : projectType(settings::VXAce)
    , gameProjectPath("")
    , languages()
    , defaultLanguage("ja")
    , isFirstExported(false)
    , langscoreProjectDirectory("")
{
}

void settings::setGameProjectPath(QString absoluteGameProjectPath)
{
    absoluteGameProjectPath.replace("\\", "/");

    auto projType = settings::getProjectType(absoluteGameProjectPath);

    if(this->gameProjectPath == absoluteGameProjectPath){ return; }
    if(projType == ProjectType::None){ return; }

    this->gameProjectPath = absoluteGameProjectPath;
    this->projectType = projType;

    if(projType == ProjectType::VXAce){
        this->writeObj.exportDirectory = "./Data/Translate";
    }
    else if(projType == ProjectType::MV || projType == ProjectType::MZ){
        this->writeObj.exportDirectory = "./data/translate";
    }

    updateLangscoreProjectPath();
}

void settings::setupLanguages(const std::vector<QLocale>& locales)
{
    for(const auto& locale : locales)
    {
        auto bcp47 = settings::getLowerBcp47Name(locale);
        if(std::find(this->languages.cbegin(), this->languages.cend(), bcp47) != this->languages.cend()){
            continue;
        }
        bool isDefault = (bcp47 == this->defaultLanguage);
        this->languages.emplace_back(
            settings::Language{bcp47, getDetafultFont(), isDefault }
        );
    }
}

void settings::setupFontList()
{
    //フォントの設定
    QFontDatabase::removeAllApplicationFonts();

    auto fontExtensions = QStringList() << "*.ttf" << "*.otf";
    {
        QFileInfoList files = QDir(qApp->applicationDirPath()+"/resources/fonts").entryInfoList(fontExtensions, QDir::Files);
        for(auto& info : files){
            auto path = info.absoluteFilePath();
            auto fontIndex = QFontDatabase::addApplicationFont(path);
            auto familiesList = QFontDatabase::applicationFontFamilies(fontIndex);
            this->fontIndexList.emplace_back(settings::Global, fontIndex, familiesList, path);
        }
    }
    //プロジェクトに登録されたフォント
    {
        QFileInfoList files = QDir(this->langscoreProjectDirectory+"/Fonts").entryInfoList(fontExtensions, QDir::Files);
        for(auto& info : files){
            auto path = info.absoluteFilePath();
            auto fontIndex = QFontDatabase::addApplicationFont(path);
            auto familiesList = QFontDatabase::applicationFontFamilies(fontIndex);
            this->fontIndexList.emplace_back(settings::Local, fontIndex, familiesList, path);
        }
    }
}

std::vector<settings::Font> settings::createFontList() const
{
    std::vector<settings::Font> fontList;
    settings::Font defaultFont = this->getDetafultFont();
    const auto projType = this->projectType;
    for(const auto& [type, index, family, path] : this->fontIndexList)
    {
        auto familyList = QFontDatabase::applicationFontFamilies(index);
        for(const auto& family : familyList)
        {
            auto font = QFont(family);
            std::uint32_t defaultPixelSize = font.pixelSize();

            auto familyNameLower = family.toLower();
            if(projType == settings::VXAce && familyNameLower.contains("vl gothic")){
                defaultFont.fontData = QFont(family);
                defaultFont.filePath = path;
                defaultFont.name = family;
                defaultPixelSize = this->getDefaultFontSize();
            }
            else if((projType == settings::MV || projType == settings::MZ) && familyNameLower.contains("m+ 1m")) {
                defaultFont.fontData = QFont(family);
                defaultFont.filePath = path;
                defaultFont.name = family;
                defaultPixelSize = this->getDefaultFontSize();
            }
            font.setPixelSize(defaultPixelSize);
            fontList.emplace_back(settings::Font{family, path, font, defaultPixelSize});
        }
    }

    return fontList;
}

settings::Font settings::getDetafultFont() const
{
    settings::Font defaultFont;
    defaultFont.size = this->getDefaultFontSize();

    if(this->projectType == settings::VXAce){
        defaultFont.fontData = QFont("VL Gothic");
        defaultFont.filePath = qApp->applicationDirPath()+"/resources/fonts/VL-Gothic-Regular.ttf";
        defaultFont.name = "VL Gothic";
    }
    else if((this->projectType == settings::MV || this->projectType == settings::MZ)) {
        defaultFont.fontData = QFont("M+ 1m regular");
        defaultFont.filePath = qApp->applicationDirPath()+"/resources/fonts/mplus-1m-regular.ttf";
        defaultFont.name = "M+ 1m regular";
    }

    return defaultFont;
}

settings::TextValidateTypeMap settings::getValidationCsvData(QString fileName)
{
    return this->getValidationCsvDataRef(fileName);
}

settings::TextValidateTypeMap& settings::getValidationCsvDataRef(QString fileName)
{
    auto it = std::ranges::find_if(writeObj.basicDataInfo, [&fileName](const auto& data) {
        return data.name.contains(fileName);
    });
    if(it != writeObj.basicDataInfo.end()) {
        return it->validateInfo;
    }
    static settings::TextValidateTypeMap Invalid{};
    return Invalid;
}

QString settings::getDefaultFontName() const
{
    switch(this->projectType)
    {
    case settings::VXAce:
        return "VL Gothic";
        break;
    case settings::MV:
        return "M+ 1m regular";
        break;
    case settings::MZ:
        return "VL Gothic";
        break;
    default:
        break;
    }

    return "VL Gothic";
}

uint32_t settings::getDefaultFontSize() const
{
    switch(this->projectType)
    {
    case settings::VXAce:
        return 22;
        break;
    case settings::MV:
        return 28;
        break;
    case settings::MZ:
        return 26;
        break;
    default:
        break;
    }

    return 22;
}

QString settings::translateDirectoryPath() const
{
    if(projectType == ProjectType::VXAce){
        return this->gameProjectPath + "/Data/" + this->transFileOutputDirName;
    }
    else if(projectType == ProjectType::MV || projectType == ProjectType::MZ){
        return this->gameProjectPath + "/data/" + this->transFileOutputDirName.toLower();
    }

    return this->gameProjectPath + "/data/" + this->transFileOutputDirName;
}

QString settings::analyzeDirectoryPath() const {
    return this->langscoreProjectDirectory+"/analyze";
}

QString settings::tempScriptFileDirectoryPath() const
{
    return this->analyzeDirectoryPath() + "/Scripts";
}

QString settings::tempGraphicsFileDirectoryPath() const
{
    if(projectType == ProjectType::VXAce){
        return this->gameProjectPath + "/Graphics";
    }
    else if(projectType == ProjectType::MV || projectType == ProjectType::MZ){
        return this->gameProjectPath + "/img";
    }
    return this->gameProjectPath + "/Graphics";
}

bool settings::isWritedLangscorePlugin() const
{
    if(this->projectType == settings::VXAce)
    {
        auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath()+"/_list.csv");
        return std::find_if(scriptList.cbegin(), scriptList.cend(), [](const auto& x){ return x[1] == "langscore"; }) != scriptList.cend();
    }
    else if(this->projectType == settings::MV || this->projectType == settings::MZ){
        return QFileInfo::exists(this->gameProjectPath + "/js/plugins/Langscore.js");
    }
    return false;
}

bool settings::isWritedLangscoreCustomPlugin() const
{
    if(this->projectType == settings::VXAce)
    {
        auto scriptList = langscore::readCsv(this->tempScriptFileDirectoryPath()+"/_list.csv");
        return std::find_if(scriptList.cbegin(), scriptList.cend(), [](const auto& x){ return x[1] == "langscore_custom"; }) != scriptList.cend();
    }
    else if(this->projectType == settings::MV || this->projectType == settings::MZ){
        return QFileInfo::exists(this->gameProjectPath + "/js/plugins/LangscoreCustom.js");
    }

    return false;
}

settings::Language& settings::fetchLanguage(QString bcp47Name)
{
    auto result = std::find(this->languages.begin(), this->languages.end(), bcp47Name);
    if(result != this->languages.end()){
        return *result;
    }
    this->languages.emplace_back(
        settings::Language{ bcp47Name, getDetafultFont(), false }
    );
    return this->languages[this->languages.size() - 1];
}

void settings::removeLanguage(QString bcp47Name)
{
    auto result = std::find(this->languages.begin(), this->languages.end(), bcp47Name);
    if(result == this->languages.end()){ return; }
    this->languages.erase(result);
}

settings::BasicData& settings::fetchBasicDataInfo(QString fileName)
{
    fileName = langscore::withoutAllExtension(fileName);
    if(fileName.isEmpty()){
        throw "Load Invalid Basic Script Info";
    }

    fileName += ".json";
    auto& list = writeObj.basicDataInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& x){
        return x.name == name;
    });

    if(result != list.end()){
        return *result;
    }

    list.emplace_back(
        settings::BasicData{fileName, false, 0}
    );
    return list[list.size() - 1];
}

settings::ScriptInfo &settings::fetchScriptInfo(QString fileName)
{
    fileName = langscore::withoutAllExtension(fileName);
    if(fileName.isEmpty()){
        throw "Load Invalid Script Info";
    }

    fileName += scriptExt(projectType);
    auto& list = writeObj.scriptInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& script){
        return script.name == name;
    });

    if(result != list.end()){
        return *result;
    }

    list.emplace_back(
        settings::ScriptInfo{{fileName, false, 0}, {}}
    );
    return list[list.size() - 1];
}

void settings::removeScriptInfoPoint(QString fileName, std::pair<size_t,size_t> point)
{
    fileName = QFileInfo(fileName).completeBaseName() + scriptExt(projectType);
    auto& list = writeObj.scriptInfo;
    auto result = std::find_if(list.begin(), list.end(), [name = fileName](const auto& script){
        return script.name == name;
    });
    if(result != list.end()){
        auto rm_result = std::remove(result->ignorePoint.begin(), result->ignorePoint.end(), point);
        result->ignorePoint.erase(rm_result, result->ignorePoint.end());
    }
}

QString settings::getLowerBcp47Name(QLocale locale)
{
    auto bcp47Name = locale.bcp47Name().toLower();
    if(bcp47Name == "zh"){ bcp47Name = "zh-cn"; }
    return bcp47Name;
}

void settings::setPackingDirectory(QString validatePath)
{
    if(validatePath.isEmpty()){
        validatePath = this->writeObj.exportDirectory;
    }

    QFileInfo outputDirInfo(validatePath);
    if(outputDirInfo.isRelative()){
        auto lsProjDir = QDir(this->langscoreProjectDirectory);
        auto absPath = lsProjDir.absoluteFilePath(validatePath);
        absPath = lsProjDir.cleanPath(absPath);
        validatePath = absPath;
    } else {
        validatePath = outputDirInfo.absoluteFilePath();
    }
    this->packingInputDirectory = validatePath;
}

void settings::updateLangscoreProjectPath()
{
    //ゲームプロジェクトファイルと同階層に Project_langscore というフォルダを作成する。
    //ゲームプロジェクトと同じフォルダに含めると、アーカイブ作成時にファイルが上手く含まれない場合がある。
    auto folderName = this->gameProjectPath.sliced(this->gameProjectPath.lastIndexOf("/")+1);
    auto gameProjPath = std::filesystem::path(this->gameProjectPath.toLocal8Bit().toStdString());
    auto langscoreProj = gameProjPath / ".." / (folderName.toLocal8Bit().toStdString() + "_langscore");
    auto u8Path = std::filesystem::absolute(langscoreProj).u8string();
    this->langscoreProjectDirectory = QString::fromStdString({u8Path.begin(), u8Path.end()});;
    this->langscoreProjectDirectory.replace("\\", "/");
}

settings::ProjectType settings::getProjectType(const QString& filePath)
{
    QFileInfo info(filePath);
    if(info.isFile() && projectExtensionAndType.contains(info.suffix())){
        return projectExtensionAndType[info.suffix()];
    }
    else if(info.isDir()){
        QDir dir(filePath);
        auto files = dir.entryInfoList();
        if(files.empty()){ return settings::None; }

        for(auto& child : files){
            if(projectExtensionAndType.contains(child.suffix())){
                return projectExtensionAndType[child.suffix()];
            }
        }
    }
    return settings::None;
}
