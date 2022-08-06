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

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::rejected);

    connect(this->ui->pushButton, &QPushButton::clicked, this, [this, setting = settings](){
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Export Directory"), setting->translateDirectoryPath());
        if(path.isEmpty()){ return; }
        this->ui->lineEdit->setText(path);
    });

    connect(this->ui->lineEdit, &QLineEdit::textEdited, this, &WriteDialog::changeOutputPath);

    this->setOutputPath(settings->writeObj.exportDirectory);

    this->ui->overwriteWarning->setHidden(true);
    this->ui->overwriteWarningPicture->setHidden(true);
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

void WriteDialog::changeOutputPath(const QString &text)
{
    auto fileInfo = QFileInfo(text);
    if(fileInfo.exists() == false){
        this->ui->overwriteWarning->setHidden(true);
        this->ui->overwriteWarningPicture->setHidden(true);
        return;
    }

    if(fileInfo.isDir() == false){
        this->ui->overwriteWarning->setVisible(true);
        this->ui->overwriteWarningPicture->setVisible(true);
        this->setToolTip(tr("The path must be a directory!"));
        return;
    }
    auto dir = QDir(text);
    auto fileInfoList = dir.entryInfoList(QDir::Filter::Files);
    for(const auto& info : fileInfoList){
        if(info.suffix() != "csv"){ continue; }

        this->ui->overwriteWarning->setVisible(true);
        this->ui->overwriteWarningPicture->setVisible(true);
        return;
    }


    this->ui->overwriteWarning->setHidden(true);
    this->ui->overwriteWarningPicture->setHidden(true);

}
