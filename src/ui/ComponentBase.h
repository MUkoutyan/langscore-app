#pragma once
#include <memory>
#include "../settings.h"
#include <QCoreApplication>
#include <QSettings>
#include <QUndoStack>
#include "ColorTheme.h"

class ComponentBase
{
public:

    enum DispatchType {
        StatusMessage,  //文字列, 秒数
        SaveProject,    //無し
        ChangeColor     //Theme
    };

    ComponentBase():setting(std::make_shared<settings>()), history(new QUndoStack), dispatchComponentList(std::make_shared<std::vector<ComponentBase*>>()){}
    ComponentBase(ComponentBase* t):setting(t->setting), history(t->history), dispatchComponentList(t->dispatchComponentList){}

    ComponentBase(const ComponentBase&) = delete;
    ComponentBase(ComponentBase&&) = delete;

    virtual ~ComponentBase(){}

    static QSettings getAppSettings() {
        return {qApp->applicationDirPath()+"/settings.ini", QSettings::IniFormat};
    }

    static ColorTheme& getColorTheme(){
        return colorTheme;
    }

    void showStatusMessage(const QString &text, int timeout = 0);

protected:

    std::shared_ptr<settings> setting;
    QUndoStack* history;
    std::shared_ptr<std::vector<ComponentBase*>> dispatchComponentList;

    void addDispatch(ComponentBase* c){
        const auto& list = *dispatchComponentList;
        if(std::find(list.begin(), list.end(), c) == list.cend()){
            (*dispatchComponentList).emplace_back(c);
        }
    }

    void removeDispatch(ComponentBase* c){
        auto& list = *dispatchComponentList;
        std::erase_if(list, [c](auto* _c){ return _c == c; });
    }

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
private:
    static ColorTheme colorTheme;

#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};
