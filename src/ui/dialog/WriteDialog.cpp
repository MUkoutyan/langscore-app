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
    //ID は divisi の MergeTextMode の値。0 (AcceptSource) は既存のCSVをそのまま残すため使わない
    this->ui->buttonGroup->setId(this->ui->acceptTarget, 1);

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
    if(result.endsWith('/')){
        result.chop(1);
    }
    return result;
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
    //出力先が空のままでは書き出せない
    this->ui->buttonBox->button(QDialogButtonBox::Ok)->setEnabled(text.trimmed().isEmpty() == false);
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
        //先に fileInfo() を呼ぶと最初のファイルを調べられないため、next() で次のファイルに進めてから情報を取得する
        auto info = QFileInfo(it.next());
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
