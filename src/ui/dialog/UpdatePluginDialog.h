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

protected:
    bool event(QEvent* event) override;

private:
    Ui::UpdatePluginDialog *ui;
    std::shared_ptr<settings> setting;
    int _writeMode;
    //開いたときのデバッグ用プラグインの設定。変更した場合は上書きが無くても書き出せる
    bool _initialEnableRemoteHub;
    //ゲームにプラグインを追加するときは true。追加では上書きするものが無い
    bool _isAddingPlugin;

    void checkButtonStatus();

#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

