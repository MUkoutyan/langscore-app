#include "ValidationFilterButton.h"
#include <QMenu>
#include "service/LanguageNames.h"

namespace
{
    const char* HiddenLanguagesKey = "ValidationFilter/hiddenLanguages";
    const char* HideErrorsKey = "ValidationFilter/hideErrors";
    const char* HideWarningsKey = "ValidationFilter/hideWarnings";
}

ValidationFilterButton::ValidationFilterButton(ComponentBase* component, QWidget* parent)
    : QToolButton(parent)
    , ComponentBase(component)
{
    {
        auto appSettings = ComponentBase::getAppSettings();
        auto& data = *this->runtimeData;
        const auto languages = appSettings.value(HiddenLanguagesKey).toStringList();
        data.hiddenErrorLanguages = QSet<QString>(languages.begin(), languages.end());
        data.hideErrors = appSettings.value(HideErrorsKey, false).toBool();
        data.hideWarnings = appSettings.value(HideWarningsKey, false).toBool();
    }

    this->setText(tr("Filter"));
    this->setToolTip(tr("Filter validation results"));
    this->setPopupMode(QToolButton::InstantPopup);
    auto* filterMenu = new QMenu(this);
    this->setMenu(filterMenu);
    //項目は開くたびに有効な言語から作成する
    connect(filterMenu, &QMenu::aboutToShow, this, [this, filterMenu]()
    {
        filterMenu->clear();
        const auto addToggle = [this, filterMenu](const QString& text, bool shown, std::function<void(bool)> apply) {
            auto* action = filterMenu->addAction(text);
            action->setCheckable(true);
            action->setChecked(shown);
            connect(action, &QAction::toggled, this, [this, apply](bool checked) {
                apply(checked);
                const auto& data = *this->runtimeData;
                auto appSettings = ComponentBase::getAppSettings();
                appSettings.setValue(HiddenLanguagesKey, QStringList(data.hiddenErrorLanguages.begin(), data.hiddenErrorLanguages.end()));
                appSettings.setValue(HideErrorsKey, data.hideErrors);
                appSettings.setValue(HideWarningsKey, data.hideWarnings);
                this->dispatch(ChangeValidationFilter, {});
            });
        };
        auto& data = *this->runtimeData;
        addToggle(tr("Errors"), data.hideErrors == false, [&data](bool shown) { data.hideErrors = (shown == false); });
        addToggle(tr("Warnings"), data.hideWarnings == false, [&data](bool shown) { data.hideWarnings = (shown == false); });
        filterMenu->addSeparator();
        for(const auto& lang : this->setting->languages)
        {
            if(lang.enable == false) { continue; }
            const auto name = lang.languageName;
            addToggle(langscore::languageDisplayName(name) + "(" + name + ")", data.hiddenErrorLanguages.contains(name) == false, [&data, name](bool shown) {
                if(shown) { data.hiddenErrorLanguages.remove(name); }
                else { data.hiddenErrorLanguages.insert(name); }
            });
        }
    });
}
