#pragma once
#include <memory>
#include "../settings.h"
#include <QCoreApplication>
#include <QSettings>
#include <QUndoStack>

class ComponentBase
{
public:
    ComponentBase():setting(std::make_shared<settings>()), history(new QUndoStack){}
    ComponentBase(ComponentBase* t):setting(t->setting), history(t->history){}

    ComponentBase(const ComponentBase&) = delete;
    ComponentBase(ComponentBase&&) = delete;

    virtual ~ComponentBase(){}

    static QSettings getAppSettings() {
        return {qApp->applicationDirPath()+"/settings.ini", QSettings::IniFormat};
    }

protected:

    std::shared_ptr<settings> setting;
    QUndoStack* history;

    void discardCommand(std::vector<QUndoCommand*> commands){
        for(auto* command : commands){
            delete command;
        }
        commands.clear();
    }
};
