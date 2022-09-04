#include "PackingMode.h"
#include "ui_PackingMode.h"
#include "../invoker.h"

#include <QScrollBar>

PackingMode::PackingMode(ComponentBase *setting, QWidget *parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::PackingMode)
    , _invoker(new invoker(this))
{
    ui->setupUi(this);

    connect(this->ui->moveToWrite, &QToolButton::clicked, this, &PackingMode::toPrevPage);
    connect(this->ui->validateButton, &QPushButton::clicked, this, &PackingMode::validate);
    connect(this->ui->packingButton, &QPushButton::clicked, this, &PackingMode::packing);


    connect(_invoker, &invoker::getStdOut, this, [this](QString text){
        text.replace("\r\n", "\n");
        this->ui->textBrowser->insertPlainText(text);
        auto vBar = this->ui->textBrowser->verticalScrollBar();
        vBar->setValue(vBar->maximum());
        this->update();
    });
    connect(_invoker, &invoker::finish, this, [this](int)
    {
        this->ui->validateButton->setEnabled(true);
        this->ui->packingButton->setEnabled(true);
    });
}

PackingMode::~PackingMode()
{
    delete ui;
}

void PackingMode::validate()
{
    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);
    _invoker->validate(true);
}

void PackingMode::packing()
{
    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);
    _invoker->packing(true);
}
