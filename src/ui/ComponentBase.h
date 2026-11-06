#pragma once
#include <memory>
#include "../settings.h"
#include <QCoreApplication>
#include <QSettings>
#include <QUndoStack>
#include <QSet>
#include "service/ColorTheme.h"
#include "service/ValidationErrorInfo.h"

class ComponentBase
{
public:
    struct RuntimeData {
        std::unordered_map<QString, std::vector<ValidationErrorInfo>> errors;
        std::unordered_map<QString, bool> updateList;
        //errors を作り直したら増やす。セルごとの検証結果の索引（CSVEditorTableModel）を作り直すために使う
        quint64 errorsRevision = 0;

        //検証結果の表示で隠す言語と種類
        QSet<QString> hiddenErrorLanguages;
        bool hideErrors = false;
        bool hideWarnings = false;

        //表示の絞り込みで隠していない検証結果か。言語の無い結果（ファイル全体）は種類だけで決める
        bool isErrorShown(const ValidationErrorInfo& info) const
        {
            if(info.type == ValidationErrorInfo::Error && hideErrors) { return false; }
            if(info.type == ValidationErrorInfo::Warning && hideWarnings) { return false; }
            const auto languages = info.language.split(' ', Qt::SkipEmptyParts);
            if(languages.isEmpty()) { return true; }
            return std::ranges::any_of(languages, [this](const QString& lang) { return hiddenErrorLanguages.contains(lang) == false; });
        }
    };

    enum DispatchType {
        StatusMessage,  //文字列, 秒数
        SaveProject,    //無し
        EnableBlur,
        DisableBlur,
        ChangeColor,    //Theme
        ValidateCSV,
        ValidateAll,
        NotifyFinishValidateCSV,
        SendLog,
        ChangeValidationFilter, //無し。検証結果の表示の絞り込みを変えた
    };

    ComponentBase()
        : setting(std::make_shared<settings>()), runtimeData(std::make_shared<RuntimeData>())
        , history(new QUndoStack), dispatchComponentList(std::make_shared<std::vector<ComponentBase*>>()){}
    ComponentBase(ComponentBase* t)
        : setting(t->setting), runtimeData(t->runtimeData), history(t->history)
        , dispatchComponentList(t->dispatchComponentList){}

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
    std::shared_ptr<RuntimeData> runtimeData;
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
