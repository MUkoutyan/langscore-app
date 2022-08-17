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
        StatusMessage,
        SaveProject
    };

    ComponentBase():setting(std::make_shared<settings>()), history(new QUndoStack), dispatchComponentList(std::make_shared<std::vector<ComponentBase*>>()){}
    ComponentBase(ComponentBase* t):setting(t->setting), history(t->history), dispatchComponentList(t->dispatchComponentList){}

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
    std::shared_ptr<std::vector<ComponentBase*>> dispatchComponentList;

    void dispatch(DispatchType type, QVariantList args){
        if(dispatchComponentList == nullptr){ return; }
        for(auto* c : *dispatchComponentList){
            c->receive(type, args);
        }
    }

    virtual void receive(DispatchType type, const QVariantList& args){ Q_UNUSED(type); Q_UNUSED(args); }

    void discardCommand(std::vector<QUndoCommand*> commands){
        for(auto* command : commands){
            delete command;
        }
        commands.clear();
    }


#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};
