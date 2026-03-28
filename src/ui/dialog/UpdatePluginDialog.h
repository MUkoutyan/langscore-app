#pragma once

#include "PopupDialogBase.h"
#include "../settings.h"

namespace Ui {
class UpdatePluginDialog;
}

class UpdatePluginDialog : public PopupDialogBase
{
    Q_OBJECT

public:
    explicit UpdatePluginDialog(std::shared_ptr<settings> setting, QWidget *parent = nullptr);
    ~UpdatePluginDialog();

    bool isEnableLanguagePatch() const;

private:
    Ui::UpdatePluginDialog *ui;
    std::shared_ptr<settings> setting;
    int _writeMode;

    void checkButtonStatus();

#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

