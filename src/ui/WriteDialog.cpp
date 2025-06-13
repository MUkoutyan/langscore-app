#include "WriteDialog.h"
#include "ui_WriteDialog.h"
#include "../csv.hpp"
#include <QFileDialog>
#include <QActionGroup>
#include <QDirIterator>

WriteDialog::WriteDialog(std::shared_ptr<settings> settings, QWidget *parent) :
    PopupDialogBase(parent),
    ui(new Ui::WriteDialog),
    _writeMode(0)
{
    ui->setupUi(this);

    //無効機能
    this->ui->keepBoth->setHidden(true);

    this->setObjectName("writeDialog");

    auto okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Write"));

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this->ui->pushButton, &QPushButton::clicked, this, [this, setting = settings](){
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Export Directory"), setting->translateDirectoryPath());
        if(path.isEmpty()){ return; }
        this->ui->lineEdit->setText(path);
    });

    this->ui->buttonGroup->setId(this->ui->keepSource, 2);
    this->ui->buttonGroup->setId(this->ui->keepBoth, 4);
    this->ui->buttonGroup->setId(this->ui->acceptTarget, 0);

    connect(this->ui->buttonGroup, &QButtonGroup::idToggled, this, [this](int id, bool check){
        this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
        this->_writeMode = id;
    });

    connect(this->ui->lineEdit, &QLineEdit::textChanged, this, &WriteDialog::changeOutputPath);
    QFileInfo outputDirInfo(settings->writeObj.exportDirectory);
    if(outputDirInfo.isRelative()){
        //langscoreのフォルダからの相対パスと解釈する。
        auto lsProjDir = QDir(settings->langscoreProjectDirectory);
        auto absPath = lsProjDir.absoluteFilePath(settings->writeObj.exportDirectory);
        absPath = lsProjDir.cleanPath(absPath);
        this->setOutputPath(absPath);
    } else {
        this->setOutputPath(outputDirInfo.absoluteFilePath());
    }

    this->ui->exportByLangCheck->setChecked(settings->writeObj.enableLanguagePatch);

    this->update();
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
    auto result = this->ui->lineEdit->text();
    if(result.last(1) == "/"){
        result.chop(1);
    }
    return result;
}

bool WriteDialog::writeByLanguagePatch() const {
    return this->ui->exportByLangCheck->isChecked();
}

int WriteDialog::writeMode() const
{
    return this->_writeMode;
}

bool WriteDialog::backup() const
{
    return this->ui->backupCheck->isChecked();
}

void WriteDialog::changeOutputPath(const QString &text)
{
    this->ui->baseWidget->setHidden(true);
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    auto fileInfo = QFileInfo(text);
    if(fileInfo.exists() == false){
        return;
    }

    if(fileInfo.isDir() == false){
        this->ui->baseWidget->setVisible(true);
        this->ui->keepSource->setVisible(false);
//        this->ui->keepBoth->setVisible(false);
        this->ui->acceptTarget->setVisible(false);
        this->setToolTip(tr("The path must be a directory!"));
        return;
    }
    QDirIterator it(text, QStringList() << "*.csv", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        auto info = it.fileInfo();
        it.next();
        if(info.suffix() != "csv"){ continue; }

        this->ui->baseWidget->setVisible(true);
        this->ui->keepSource->setVisible(true);
        this->ui->keepBoth->setVisible(true);
        this->ui->acceptTarget->setVisible(true);
        if(this->ui->buttonGroup->checkedButton() == nullptr){
            this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        }
        break;
    }
}
