#include "PackingMode.h"
#include "src/utility.hpp"
#include "ui_PackingMode.h"
#include "../invoker.h"
#include "../csv.hpp"
#include "../settings.h"

#include <QFileInfo>
#include <QScrollBar>
#include <QTimer>
#include <QTreeWidget>
#include <QProcess>
#include <QDir>

enum ErrorTextCol
{
    Type = 0,
    Summary,
    Details,
    File,
    Row,
};

PackingMode::PackingMode(ComponentBase *setting, QWidget *parent)
    : QWidget{parent}
    , ComponentBase(setting)
    , ui(new Ui::PackingMode)
    , _invoker(new invoker(this))
    , _finishInvoke(false)
{
    ui->setupUi(this);

    connect(this->ui->moveToWrite, &QToolButton::clicked, this, &PackingMode::toPrevPage);
    connect(this->ui->validateButton, &QPushButton::clicked, this, &PackingMode::validate);
    connect(this->ui->packingButton, &QPushButton::clicked, this, &PackingMode::packing);


    connect(_invoker, &invoker::getStdOut, this, [this](QString raw){
        raw.replace("\r\n", "\n");
        auto list = raw.split("\n");
        for(auto&& text : list){
            this->addText(std::move(text));
        }
    });
    connect(_invoker, &invoker::finish, this, [this](int)
    {
        this->ui->validateButton->setEnabled(true);
        this->ui->packingButton->setEnabled(true);
        this->ui->moveToWrite->setEnabled(true);
        this->ui->treeWidget->blockSignals(false);
        this->_finishInvoke = true;
    });

    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, [this]()
    {
        auto selectedItems = this->ui->treeWidget->selectedItems();
        if(selectedItems.empty()){ return; }
        auto item = selectedItems[0];
        this->setupCsvTable(item->parent() ? item->parent()->text(0) : item->text(0));
    });

    this->treeMenu = new QMenu(this->ui->treeWidget);
//    auto openFile = treeMenu->addAction(tr("Open File"));
    auto openExplorer = treeMenu->addAction(tr("Show in Explorer"));
    connect(this->ui->treeWidget, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint&)
    {
        this->treeMenu->exec(QCursor::pos());
    });
//    connect(openFile, &QAction::triggered, this, [this]()
//    {
//        auto filePath = this->getCurrentSelectedItemFilePath();
//        if(QFile(filePath).exists() == false){ return; }
//        qDebug() << "start" QDir::toNativeSeparators(filePath);
//        QProcess::startDetached("start", {QDir::toNativeSeparators(filePath)});
//    });
    connect(openExplorer, &QAction::triggered, this, [this]()
    {
        auto filePath = this->getCurrentSelectedItemFilePath();
        if(QFile(filePath).exists() == false){ return; }
        QProcess::startDetached("explorer", {"/select,"+QDir::toNativeSeparators(filePath)});
    });

}

PackingMode::~PackingMode()
{
    delete ui;
}

QString PackingMode::getCurrentSelectedItemFilePath()
{
    auto selectItems = this->ui->treeWidget->selectedItems();
    if(selectItems.empty()){ return ""; }
    auto item = selectItems[0];
    auto fileName = item->text(0);
    auto filePath = this->setting->translateDirectoryPath() + "/" + fileName+".csv";
    return filePath;
}

void PackingMode::setupCsvTable(QString fileName)
{
    const auto translateFolder = this->setting->translateDirectoryPath();
    auto csv = langscore::readCsv(translateFolder + "/" + langscore::withoutExtension(fileName) + ".csv");
    if(csv.empty()){ return; }

    const auto& header = csv[0];
    auto numTableItems = csv.size()-1;
    QTableWidget *targetTable = this->ui->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->setRowCount(numTableItems);
    targetTable->setColumnCount(header.size());
    for(int c=0; c<header.size(); ++c){
        auto* item = new QTableWidgetItem(header[c]);
        targetTable->setHorizontalHeaderItem(c, item);
    }

    for(int r=0; r<numTableItems; ++r)
    {
        const auto& line = csv[r+1];
        for(int c=0; c<line.size(); ++c){
            auto text = line[c];
            auto* item = new QTableWidgetItem(text);
            targetTable->setItem(r, c, item);
        }
    }

}

void PackingMode::validate()
{
    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);
    this->ui->moveToWrite->setEnabled(false);
    this->ui->treeWidget->blockSignals(true);
    this->ui->treeWidget->clear();
    _invoker->validate();
    _finishInvoke = false;
    updateTimer = new QTimer();
    connect(updateTimer, &QTimer::timeout, this, &PackingMode::updateTree);
    updateTimer->start(100);
}

void PackingMode::packing()
{
    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);
    this->ui->moveToWrite->setEnabled(false);
    _invoker->packing();
}

void PackingMode::addText(QString text)
{
    if(text.isEmpty()){ return; }
    auto cols = langscore::parseCsvLine(text);

    ErrorInfo info;

    auto typeText = cols[ErrorTextCol::Type];
    if(typeText == "Warning"){
        info.type = ErrorInfo::Warning;
    }
    else if(typeText == "Error"){
        info.type = ErrorInfo::Error;
    }
    else {
        info.type = ErrorInfo::Invalid;
        return;
    }

    auto summaryRaw = cols[ErrorTextCol::Summary].toInt();
    if(ErrorInfo::EmptyCol <= summaryRaw && summaryRaw <= ErrorInfo::NotFoundEsc){
        info.summary = static_cast<ErrorInfo::ErrorSummary>(summaryRaw);
    }
    else{
        info.type = ErrorInfo::Invalid;
        return;
    }

    info.detail = cols[ErrorTextCol::Details];
    info.row = cols[ErrorTextCol::Row].toULongLong();

    QFileInfo fileInfo(cols[ErrorTextCol::File]);
    auto fileName = langscore::withoutExtension(fileInfo.fileName());
    _mutex.lock();
    errors[fileName].emplace_back(std::move(info));
    updateList[fileName] = true;
    _mutex.unlock();

    this->update();
}

void PackingMode::updateTree()
{
    bool updated = false;
    for(auto& updateInfo : updateList)
    {
        if(updateInfo.second == false){ continue; }
        updated = true;

        std::lock_guard<std::mutex> locker(_mutex);
        auto& infoList = errors[updateInfo.first];
        for(auto& info : infoList)
        {
            if(info.shown){ continue; }

            QTreeWidgetItem* item = nullptr;
            if(treeTopItemList.find(updateInfo.first) == treeTopItemList.end()){
                item = new QTreeWidgetItem();
                auto size = item->sizeHint(0);
                size.setHeight(50);
                item->setSizeHint(0, size);
                item->setText(0, updateInfo.first);
                if(info.type == ErrorInfo::Warning){
                    item->setIcon(0, QIcon(":/images/resources/image/warning.svg"));
                }
                else if(info.type == ErrorInfo::Error){
                    item->setIcon(0, QIcon(":/images/resources/image/attention.svg"));
                }
                treeTopItemList[updateInfo.first] = item;
                this->ui->treeWidget->addTopLevelItem(item);
            }
            else {
                item = treeTopItemList[updateInfo.first];
            }

            auto child = new QTreeWidgetItem();
            [&](QTreeWidgetItem* item){
                QString text;
                if(info.type == ErrorInfo::Warning){
                    text += tr("[Warning]");
                    item->setForeground(0, QColor("#f0e300"));
                }
                else if(info.type == ErrorInfo::Error){
                    text += tr("[Error]");
                    item->setForeground(0, QColor("#ec0b00"));
                }
                if(info.summary == ErrorInfo::EmptyCol){
                    text += tr(" Empty Column (") + info.detail + ")";
                }
                else if(info.summary == ErrorInfo::NotFoundEsc){
                    text += tr(" Not Found Esc (") + info.detail + ")";
                }

                child->setText(0, tr("Line") + QString::number(info.row)+" : " + text);
            }(child);
            item->addChild(child);
            info.shown = true;
        }
        updateInfo.second = false;
    }

    if(_finishInvoke && updated == false){
        delete this->updateTimer;
        this->updateTimer = nullptr;
    }
}
