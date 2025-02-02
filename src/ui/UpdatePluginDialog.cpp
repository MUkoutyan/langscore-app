#include "UpdatePluginDialog.h"
#include "ui_UpdatePluginDialog.h"
#include "../csv.hpp"
#include <QFileDialog>
#include <QActionGroup>
#include <QPushButton>

UpdatePluginDialog::UpdatePluginDialog(std::shared_ptr<settings> settings, QWidget *parent) :
    PopupDialogBase(parent),
    ui(new Ui::UpdatePluginDialog),
    _writeMode(0)
{
    ui->setupUi(this);

    this->setObjectName("UpdatePluginDialog");

    auto okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Write"));

    this->ui->updateLsScript->setEnabled(true);
    this->ui->updateLsScript->setChecked(settings->writeObj.overwriteLangscore);
    connect(this->ui->updateLsScript, &QCheckBox::clicked, this, [setting = settings](bool check){
        setting->writeObj.overwriteLangscore = check;
    });

    this->ui->updateLsCustomScript->setChecked(settings->writeObj.overwriteLangscoreCustom);
    connect(this->ui->updateLsCustomScript, &QCheckBox::clicked, this, [setting = settings](bool check){
        setting->writeObj.overwriteLangscoreCustom = check;
    });

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    this->ui->enablePatch->setChecked(settings->writeObj.enableLanguagePatch);

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
