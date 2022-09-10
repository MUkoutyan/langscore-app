﻿#include "PackingMode.h"
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
    Language,
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
    , errorInfoIndex(0)
    , currentShowCSV("")
{
    ui->setupUi(this);

    this->ui->modeDesc->hide();
    this->ui->treeWidget->setStyleSheet("QTreeView::item { height: 28px; }");
    this->ui->splitter->setStretchFactor(1, 1);

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
        this->ui->packingButton->setEnabled(true);
        this->ui->moveToWrite->setEnabled(true);
        this->ui->treeWidget->blockSignals(false);
        this->ui->tableWidget->blockSignals(false);
        this->_finishInvoke = true;

        if(this->ui->treeWidget->topLevelItemCount() == 0){
            this->dispatch(DispatchType::StatusMessage, {tr("Valid!"), 5000});
        }
    });

    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, [this]()
    {
        auto selectedItems = this->ui->treeWidget->selectedItems();
        if(selectedItems.empty()){ return; }
        this->ui->tableWidget->clearSelection();
        auto item = selectedItems[0];
        if(item->parent()){
            this->setupCsvTable(item->parent()->text(0));
            this->highlightError(item);
        }
        else{
            this->setupCsvTable(item->text(0));
        }
    });

    this->treeMenu = new QMenu(this->ui->treeWidget);
    auto openExplorer = treeMenu->addAction("");
    connect(this->ui->treeWidget, &QTreeWidget::customContextMenuRequested, this, [this, openExplorer](const QPoint&)
    {
        auto filePath = this->getCurrentSelectedItemFilePath();
        if(QFile(filePath).exists() == false){ return; }
        openExplorer->setText(tr("Show in Explorer") + " (" + QFileInfo(filePath).fileName() + ")");
        this->treeMenu->exec(QCursor::pos());
    });

//    auto openFile = treeMenu->addAction(tr("Open File"));
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
    auto fileName = item->parent() ? item->parent()->text(0) : item->text(0);
    auto filePath = this->setting->translateDirectoryPath() + "/" + fileName+".csv";
    return filePath;
}

void PackingMode::setupCsvTable(QString fileName)
{
    if(currentShowCSV == fileName){ return; }
    const auto translateFolder = this->setting->translateDirectoryPath();
    auto csv = langscore::readCsv(translateFolder + "/" + langscore::withoutExtension(fileName) + ".csv");
    if(csv.empty()){ return; }

    const auto& header = QStringList{csv[0].begin(), csv[0].end()};
    auto numTableItems = csv.size()-1;
    QTableWidget *targetTable = this->ui->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->setRowCount(numTableItems);
    const auto numColumns = header.size();
    targetTable->setColumnCount(numColumns);
    for(int c=0; c<header.size(); ++c){
        auto* item = new QTableWidgetItem(header[c]);
        targetTable->setHorizontalHeaderItem(c, item);
    }

    auto errorInfoList = this->errors[fileName];

    for(int r=0; r<numTableItems; ++r)
    {
        const auto& line = csv[r+1];
        for(int c=0; c<numColumns; ++c)
        {
            auto text = line.size() <= c ? "" : line[c];
            auto* item = new QTableWidgetItem(text);
            targetTable->setItem(r, c, item);

            auto result = std::find_if(errorInfoList.begin(), errorInfoList.end(), [&](const ErrorInfo& x){
                auto targetLangs = x.language.split(" ");
                return x.row == r+1 && targetLangs.contains(header[c]);
            });
            if(result != errorInfoList.end())
            {
                item->setData(Qt::UserRole, result->id);
                item->setBackground(result->type == ErrorInfo::Warning ? QColor(240, 227, 0, 51) : QColor(236, 11, 0, 51));
                QString tooltipStr;
                if(result->summary == ErrorInfo::EmptyCol){
                    tooltipStr = tr("The text is empty.");
                }
                else if(result->summary == ErrorInfo::NotFoundEsc){
                    tooltipStr = tr("Esc character is missing.");
                    tooltipStr += result->detail;
                }
                item->setToolTip(tooltipStr);
            }
        }
    }

    this->ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    this->currentShowCSV = fileName;

}

void PackingMode::highlightError(QTreeWidgetItem *treeItem)
{
    auto id = treeItem->data(0, Qt::UserRole).toULongLong();
    if(id == 0){ return; }

    const auto& errorList = errors[treeItem->parent()->text(0)];

    auto result = std::find_if(errorList.cbegin(), errorList.cend(), [id](const auto& x){
        return x.id ==id;
    });
    if(result == errorList.cend()){ return; }

    const auto numRow = this->ui->tableWidget->rowCount();
    const auto numCol = this->ui->tableWidget->columnCount();

    for(int r=0; r<numRow; ++r)
    {
        for(int c=0; c<numCol; ++c)
        {
            auto tableItem = this->ui->tableWidget->item(r, c);
            auto tableId = tableItem->data(Qt::UserRole).toULongLong();
            if(tableId == 0){ continue; }

            if(id == tableId){
                tableItem->setSelected(true);
                this->ui->tableWidget->scrollToItem(tableItem, QAbstractItemView::ScrollHint::PositionAtCenter);
                return;
            }
        }
    }

}

void PackingMode::validate()
{
    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);
    this->ui->moveToWrite->setEnabled(false);
    this->ui->treeWidget->blockSignals(true);
    this->ui->tableWidget->blockSignals(true);

    this->ui->treeWidget->clear();
    this->treeTopItemList.clear();
    this->ui->tableWidget->clear();
    this->ui->tableWidget->clearContents();
    this->updateList.clear();
    this->errors.clear();
    currentShowCSV = "";

    errorInfoIndex = 0;

    _finishInvoke = false;
    _invoker->validate();
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

    info.language   = cols[ErrorTextCol::Language];
    info.detail     = cols[ErrorTextCol::Details];
    info.row        = cols[ErrorTextCol::Row].toULongLong();
    info.id         = (++errorInfoIndex);   //1開始にする

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
                item->setText(0, updateInfo.first);
                //アイコンの設定　エラー優先で警告は次点
                auto error = std::find_if(infoList.cbegin(), infoList.cend(), [](const auto& x){ return x.type == ErrorInfo::Error; });
                if(error != infoList.cend()){
                    item->setIcon(0, QIcon(":/images/resources/image/attention.svg"));
                }
                else
                {
                    auto warn = std::find_if(infoList.cbegin(), infoList.cend(), [](const auto& x){ return x.type == ErrorInfo::Warning; });
                    if(warn != infoList.cend()){
                        item->setIcon(0, QIcon(":/images/resources/image/warning.svg"));
                    }
                }
                treeTopItemList[updateInfo.first] = item;
                this->ui->treeWidget->addTopLevelItem(item);
            }
            else {
                item = treeTopItemList[updateInfo.first];
            }

            auto child = new QTreeWidgetItem();
            [&](QTreeWidgetItem* item)
            {
                QString text;
                if(info.type == ErrorInfo::Warning){
                    item->setIcon(0, QIcon(":/images/resources/image/warning.svg"));
                    item->setForeground(0, QColor(240, 227, 0));
                }
                else if(info.type == ErrorInfo::Error){
                    item->setIcon(0, QIcon(":/images/resources/image/attention.svg"));
                    item->setForeground(0, QColor(236, 11, 0));
                }
                if(info.summary == ErrorInfo::EmptyCol){
                    text += tr(" Empty Column") + "[" + info.language + "]";
                }
                else if(info.summary == ErrorInfo::NotFoundEsc){
                    text += tr(" Not Found Esc") + "[" + info.language + "] (" + info.detail + ")";
                }

                child->setText(0, tr("Line") + QString::number(info.row)+" : " + text);
            }(child);
            child->setData(0, Qt::UserRole, info.id);
            item->addChild(child);
            info.shown = true;
        }
        updateInfo.second = false;
    }

    if(_finishInvoke && updated == false){
        delete this->updateTimer;
        this->updateTimer = nullptr;
        this->ui->validateButton->setEnabled(true);
    }
}
