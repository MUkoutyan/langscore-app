#include "WriteDialog.h"
#include "ui_WriteDialog.h"
#include <QFileDialog>

WriteDialog::WriteDialog(std::shared_ptr<settings> settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WriteDialog)
{
    ui->setupUi(this);

    auto okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Write"));

    this->setOutputPath(settings->writeObj.exportDirectory);

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::rejected);

    connect(this->ui->pushButton, &QPushButton::clicked, this, [this, setting = std::move(settings)](){
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Export Directory"), setting->translateDirectoryPath());
        if(path.isEmpty()){ return; }
        this->ui->lineEdit->setText(path);
    });
}

WriteDialog::~WriteDialog()
{
    delete ui;
}

void WriteDialog::setOutputPath(QString directoryPath)
{
    this->ui->lineEdit->setText(directoryPath);
}

QString WriteDialog::outputPath() const
{
    return this->ui->lineEdit->text();
}

bool WriteDialog::writeByLanguage() const {
    return this->ui->checkBox->isChecked();
}
