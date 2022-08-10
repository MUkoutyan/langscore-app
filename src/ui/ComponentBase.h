#pragma once
#include <memory>
#include "../settings.h"
#include <QCoreApplication>
#include <QSettings>
#include <QUndoStack>

class ComponentBase
{
public:

    enum DispatchType {
        StatusMessage
    };

    ComponentBase():setting(std::make_shared<settings>()), history(new QUndoStack){}
    ComponentBase(ComponentBase* t):setting(t->setting), history(t->history){}

    ComponentBase(const ComponentBase&) = delete;
    ComponentBase(ComponentBase&&) = delete;

    virtual ~ComponentBase(){}

    static QSettings getAppSettings() {
        return {qApp->applicationDirPath()+"/settings.ini", QSettings::IniFormat};
    }

    void showStatusMessage(const QString &text, int timeout = 0);

protected:

    std::shared_ptr<settings> setting;
    QUndoStack* history;
    std::vector<ComponentBase*> dispatchComponentList;

    void dispatch(DispatchType type, QVariantList args){
        for(auto* c : dispatchComponentList){
            c->receive(type, args);
        }
    }

    virtual void receive(DispatchType type, const QVariantList& args){}

    void discardCommand(std::vector<QUndoCommand*> commands){
        for(auto* command : commands){
            delete command;
        }
        commands.clear();
    }
};
