#include "UpdatePluginDialog.h"
#include "ui_UpdatePluginDialog.h"
#include "../csv.hpp"
#include <QFileDialog>
#include <QActionGroup>
#include <QPushButton>

UpdatePluginDialog::UpdatePluginDialog(std::shared_ptr<settings> settings, QWidget *parent) :
    PopupDialogBase(parent),
    ui(new Ui::UpdatePluginDialog),
    setting(std::move(settings)),
    _writeMode(0),
    _initialEnableRemoteHub(setting->writeObj.enableRemoteHub)
{
    ui->setupUi(this);

    this->setObjectName("UpdatePluginDialog");

    auto okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Write"));

    this->ui->enablePatch->setChecked(setting->writeObj.enableLanguagePatch);
    connect(this->ui->enablePatch, &QCheckBox::clicked, this, [this](bool check){
        setting->writeObj.enableLanguagePatch = check;
        this->checkButtonStatus();
    });

    this->ui->updateLsScript->setChecked(setting->writeObj.overwriteLangscore);
    connect(this->ui->updateLsScript, &QCheckBox::clicked, this, [this](bool check){
        setting->writeObj.overwriteLangscore = check;
        this->checkButtonStatus();
    });

    this->ui->updateLsCustomScript->setChecked(setting->writeObj.overwriteLangscoreCustom);
    connect(this->ui->updateLsCustomScript, &QCheckBox::clicked, this, [this](bool check){
        setting->writeObj.overwriteLangscoreCustom = check;
        this->checkButtonStatus();
    });


    //ゲームへの追従・反映で使うデバッグ用のプラグイン。プラグインの更新で追加・削除する
    this->ui->enableRemoteHub->setChecked(setting->writeObj.enableRemoteHub);
    connect(this->ui->enableRemoteHub, &QCheckBox::clicked, this, [this](bool check){
        setting->writeObj.enableRemoteHub = check;
        this->checkButtonStatus();
    });

    this->ui->enableTransDefLang->setChecked(setting->writeObj.enableTranslateDefLang);
    connect(this->ui->enableTransDefLang, &QCheckBox::clicked, this, [this](bool check){
        setting->writeObj.enableTranslateDefLang = check;
        this->checkButtonStatus();
    });

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    this->checkButtonStatus();

    this->update();
}

UpdatePluginDialog::~UpdatePluginDialog()
{
    delete ui;
}

bool UpdatePluginDialog::isEnableLanguagePatch() const
{
    return this->ui->enablePatch->isChecked();
}

void UpdatePluginDialog::checkButtonStatus()
{
    //デバッグ用のプラグインの追加・削除だけでも書き出せるようにする
    if(setting->writeObj.overwriteLangscore == false &&
       setting->writeObj.overwriteLangscoreCustom == false &&
       setting->writeObj.enableRemoteHub == _initialEnableRemoteHub){
        this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else{
        this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}
