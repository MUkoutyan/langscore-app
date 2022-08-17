#include "WriteDialog.h"
#include "ui_WriteDialog.h"
#include <QFileDialog>
#include <QActionGroup>

WriteDialog::WriteDialog(std::shared_ptr<settings> settings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::WriteDialog),
    _writeMode(0)
{
    ui->setupUi(this);
    this->setObjectName("writeDialog");
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setStyleSheet("#writeDialog{border: 2px solid #ececec;}");

    auto okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Write"));

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::rejected);

    connect(this->ui->pushButton, &QPushButton::clicked, this, [this, setting = settings](){
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Export Directory"), setting->translateDirectoryPath());
        if(path.isEmpty()){ return; }
        this->ui->lineEdit->setText(path);
    });

    this->ui->buttonGroup->setId(this->ui->overwriteMode, 1);
    this->ui->buttonGroup->setId(this->ui->truncMode, 2);

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

bool WriteDialog::writeByLanguage() const {
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
        this->ui->overwriteMode->setVisible(false);
        this->ui->truncMode->setVisible(false);
        this->setToolTip(tr("The path must be a directory!"));
        return;
    }
    auto dir = QDir(text);
    auto fileInfoList = dir.entryInfoList(QDir::Filter::Files | QDir::Filter::NoDotAndDotDot);
    for(const auto& info : fileInfoList){
        if(info.suffix() != "csv"){ continue; }

        this->ui->baseWidget->setVisible(true);
        this->ui->overwriteMode->setVisible(true);
        this->ui->truncMode->setVisible(true);

        if(this->ui->buttonGroup->checkedButton() == nullptr){
            this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(false);
        }

        return;
    }



}
