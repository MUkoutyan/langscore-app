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
    _writeMode(0)
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

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);


    this->ui->enablePatch->setEnabled(false); //無効化

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
    if(setting->writeObj.overwriteLangscore == false &&
       setting->writeObj.overwriteLangscoreCustom == false){
        this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
    }
    else{
        this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    }
}
