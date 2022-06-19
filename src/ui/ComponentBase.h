#pragma once
#include <memory>
#include "../settings.h"
#include <QCoreApplication>
#include <QSettings>

class ComponentBase
{
public:
    struct Common {
        using Type = std::shared_ptr<settings>;
        Type obj;
    };

    ComponentBase(Common::Type t):common({std::move(t)}){}
    virtual ~ComponentBase(){}

    static QSettings getSettings() {
        return {qApp->applicationDirPath()+"/settings.ini", QSettings::IniFormat};
    }

protected:
    Common common;
};
