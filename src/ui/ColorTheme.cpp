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


ColorTheme ComponentBase::colorTheme;
