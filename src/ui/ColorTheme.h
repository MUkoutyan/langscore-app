#ifndef COLORTHEME_H
#define COLORTHEME_H

#include <QObject>

class ColorTheme
{
public:
    enum Theme {
        Dark = 0,
        Light,
        System,

        None
    };

    ColorTheme();

    void restoreTheme();

    void setTheme(Theme t) noexcept;
    Theme getCurrentTheme() const noexcept;

private:
    Theme currentTheme;
};

#endif // COLORTHEME_H
