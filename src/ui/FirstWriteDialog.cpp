#include "FirstWriteDialog.h"
#include "ui_FirstWriteDialog.h"
#include <QFileDialog>
#include <QActionGroup>

FirstWriteDialog::FirstWriteDialog(std::shared_ptr<settings> settings, QWidget *parent) :
    PopupDialogBase(parent),
    ui(new Ui::FirstWriteDialog),
    _writeMode(0)
{
    ui->setupUi(this);

    this->setObjectName("FirstWriteDialog");

    auto okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);
    okButton->setText(tr("Write"));

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(this->ui->pushButton, &QPushButton::clicked, this, [this, setting = settings](){
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Export Directory"), setting->translateDirectoryPath());
        if(path.isEmpty()){ return; }
        this->ui->lineEdit->setText(path);
    });

    connect(this->ui->lineEdit, &QLineEdit::textChanged, this, &FirstWriteDialog::changeOutputPath);
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

FirstWriteDialog::~FirstWriteDialog()
{
    delete ui;
}

void FirstWriteDialog::setOutputPath(QString directoryPath)
{
    this->ui->lineEdit->setText(directoryPath);
}

QString FirstWriteDialog::outputPath() const
{
    auto result = this->ui->lineEdit->text();
    if(result.last(1) == "/"){
        result.chop(1);
    }
    return result;
}

bool FirstWriteDialog::writeByLanguagePatch() const {
    return this->ui->exportByLangCheck->isChecked();
}

int FirstWriteDialog::writeMode() const
{
    return this->_writeMode;
}

bool FirstWriteDialog::backup() const
{
    return this->ui->backupCheck->isChecked();
}

void FirstWriteDialog::changeOutputPath(const QString &text)
{
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(true);
    auto fileInfo = QFileInfo(text);
    if(fileInfo.exists() == false){
        return;
    }

    if(fileInfo.isDir() == false){
        this->setToolTip(tr("The path must be a directory!"));
        return;
    }
}
