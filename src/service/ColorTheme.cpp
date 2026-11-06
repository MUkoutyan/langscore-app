#include "ColorTheme.h"
#include <QSettings>
#include <QApplication>
#include "ComponentBase.h"


ColorTheme::ColorTheme()
    : currentTheme(Theme::System)
{
    this->restoreTheme();
}

void ColorTheme::restoreTheme()
{
    QSettings settings = {qApp->applicationDirPath()+"/settings.ini", QSettings::IniFormat};
    //未設定だった場合はSystem(2)
    auto theme = (ColorTheme::Theme)settings.value("appTheme", 2).toInt();
    this->currentTheme = theme;
}

void ColorTheme::setTheme(Theme t) noexcept
{
    this->currentTheme = t;
    QSettings settings = {qApp->applicationDirPath()+"/settings.ini", QSettings::IniFormat};
    settings.setValue("appTheme", this->currentTheme);
}

ColorTheme::Theme ColorTheme::getCurrentTheme() const noexcept
{
    ColorTheme::Theme result = this->currentTheme;
    if(result == ColorTheme::Theme::System)
    {
#ifdef Q_OS_WIN
        QSettings regist("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
        if(regist.value("AppsUseLightTheme")==0){
            result = ColorTheme::Theme::Dark;
        }
        else{
            result = ColorTheme::Theme::Light;
        }
#endif
    }
    return result;
}

QColor ColorTheme::getTopLevelItemBGColor()
{
    if(this->getCurrentTheme() == ColorTheme::Dark) {
        return QColor(128, 128, 128, 90);
    }
    return QColor(255, 255, 255, 128);
}

QColor ColorTheme::getGraphicFolderBGColor()
{
    if(this->getCurrentTheme() == ColorTheme::Dark) {
        return QColor(128, 128, 128, 90);
    }
    return QColor(255, 255, 255, 128);
}

QColor ColorTheme::getItemTextColor()
{
    if(this->getCurrentTheme() == ColorTheme::Dark) {
        return Qt::white;
    }
    return Qt::black;
}

std::unordered_map<Qt::CheckState, QColor> ColorTheme::getTextColorForState()
{
    if(this->getCurrentTheme() == ColorTheme::Dark) {
        return {
            {Qt::Checked,           QColor(0xfefefe)},
            {Qt::PartiallyChecked,  QColor(0x9a9a9a)},
            {Qt::Unchecked,         QColor(0x5a5a5a)},
        };
    }
    return {
        {Qt::Checked,           QColor(0x161616)},
        {Qt::PartiallyChecked,  QColor(0x646464)},
        {Qt::Unchecked,         QColor(0x9a9a9a)},
    };
}


ColorTheme ComponentBase::colorTheme;
