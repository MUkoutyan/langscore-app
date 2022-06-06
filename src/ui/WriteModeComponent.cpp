#include "WriteModeComponent.h"
#include "ui_WriteModeComponent.h"
#include "MainComponent.h"

#include <QFileInfo>
#include <QDir>
#include <QString>
#include <QCheckBox>

#include <filesystem>
#include <fstream>
#include <QLocale>

std::vector<std::vector<QString>> readCsv(QString path)
{
    std::ifstream file(path.toLocal8Bit());
    if(file.bad()){ return {}; }

    std::vector<QString> rows;
    std::string _line;
    QString line_buffer;
    while(std::getline(file, _line))
    {
        QString line = QString::fromStdString(_line);
        auto num_dq = std::count(line.begin(), line.end(), '\"');

        if(line_buffer.isEmpty() == false){
            num_dq += 1;
        }

        if(num_dq % 2 == 0){
            line_buffer += line;
            rows.emplace_back(std::move(line_buffer));
            line_buffer = "";
        }
        else{
            line_buffer += line+'\n';	//getlineに改行は含まれないのでここで足す
        }
    }

    bool find_dq = false;
    std::vector<std::vector<QString>> csv;
    for(auto& row : rows)
    {
        QString tmp = "";
        std::vector<QString> cols;
        for(auto c : row)
        {
            if(c == '\"'){ find_dq = !find_dq; }

            if(find_dq){
                tmp.push_back(c);
                continue;
            }

            if(c == ','){
                find_dq = false;
                cols.emplace_back(std::move(tmp));
                tmp = "";
                continue;
            }
            else if(c == '\n'){
                find_dq = false;
                cols.emplace_back(std::move(tmp));
                tmp = "";
                break;
            }
            tmp.push_back(c);
        }
        if(tmp.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

        csv.emplace_back(std::move(cols));
    }

    if(csv.size() < 2){ return {}; }

    return csv;
}

WriteModeComponent::WriteModeComponent(MainComponent *parent)
    : QWidget{parent}
    , ui(new Ui::WriteModeComponent)
    , _parent(parent)
{
    ui->setupUi(this);
}

void WriteModeComponent::show()
{
    //Languages
    std::vector<QLocale> languages = {
        {QLocale::English},
        {QLocale::Spanish},
        {QLocale::German},
        {QLocale::French},
        {QLocale::Italian},
        {QLocale::Japanese},
        {QLocale::Korean},
        {QLocale::Russian},
        {QLocale::Chinese, QLocale::SimplifiedChineseScript},
        {QLocale::Chinese, QLocale::TraditionalChineseScript},
    };

    int count = 0;
    for(const auto& lang : languages)
    {
        auto bcp47Name = lang.bcp47Name();
        if(bcp47Name == "zh"){ bcp47Name = "zh-cn"; }
        auto& langList = this->_parent->setting.languages;
        auto text = lang.nativeLanguageName() + "(" + bcp47Name.toLower() + ")";
        auto check = new QCheckBox(text);
        if(langList.contains(bcp47Name)){
            check->setChecked(true);
        }
        connect(check, &QCheckBox::toggled, this, [this, shortName = bcp47Name](bool check)
        {
            auto& langList = this->_parent->setting.languages;
            if(check && langList.contains(shortName) == false)
            {
                langList.emplace_back(shortName);
            }
            else if(check==false && langList.contains(shortName)){
                langList.removeOne(shortName);
            }
        });
        this->ui->langTabGrid->addWidget(check, count/3, count%3);
        count++;
    }

    //CSV
    this->ui->lineEdit->setText(this->_parent->setting.project);
    {
        QDir sourceDir(this->_parent->setting.outputDirectory+"/tmp");
        auto files = sourceDir.entryInfoList();

        auto mainItem = new QTreeWidgetItem();
        mainItem->setText(0, "Main");
        this->ui->treeWidget->addTopLevelItem(mainItem);
        for(const auto& file : files)
        {
            if(file.suffix() != "json"){ continue; }

            auto fileName = file.fileName();
            if(fileName == "config.json"){ continue; }
            else if(fileName == "Scripts.json"){ continue; }
            else if(fileName == "Graphics.json"){ continue; }

            auto child = new QTreeWidgetItem();
            child->setText(0, file.completeBaseName());
            child->setData(0, Qt::UserRole, file.completeBaseName());
            mainItem->addChild(child);
        }
    }
    {
        auto scriptFolder = this->_parent->setting.outputDirectory+"/tmp/Scripts/";
        auto scriptFiles = readCsv(scriptFolder + "_list.csv");
        auto scriptItem = new QTreeWidgetItem();
        scriptItem->setText(0, "Script");
        this->ui->treeWidget->addTopLevelItem(scriptItem);
        for(const auto& l : scriptFiles)
        {
            auto file = QFileInfo(scriptFolder+l[1]);
            if(file.suffix() != "rb"){ continue; }
            if(file.size() == 0){ continue; }

            auto child = new QTreeWidgetItem();
            child->setText(0, file.completeBaseName());
            child->setData(0, Qt::UserRole, file.completeBaseName());
            scriptItem->addChild(child);
        }
    }


    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, [this]()
    {
        auto selectedItems = this->ui->treeWidget->selectedItems();
        if(selectedItems.empty()){ return; }
        auto item = selectedItems[0];
        if(item->parent() == nullptr){ return; }

        if(item->parent()->text(0) == "Main"){
            this->ui->tabWidget->setCurrentIndex(1);
        }
        else if(item->parent()->text(0) == "Script"){
            this->ui->tabWidget->setCurrentIndex(2);
        }
        auto fileName = item->data(0, Qt::UserRole).toString();
        auto csv = readCsv(this->_parent->setting.project + "/Translate/" + fileName + ".csv");
        if(csv.empty()){ return; }

        const auto& header = csv[0];
        this->ui->tableWidget->clear();
        this->ui->tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        this->ui->tableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        this->ui->tableWidget->setRowCount(csv.size());
        this->ui->tableWidget->setColumnCount(header.size());
        for(int c=0; c<header.size(); ++c){
            auto* item = new QTableWidgetItem(header[c]);
            this->ui->tableWidget->setHorizontalHeaderItem(c, item);
        }

        for(int r=0; r<csv.size()-1; ++r)
        {
            const auto& line = csv[r+1];
            for(int c=0; c<line.size(); ++c){
                auto* item = new QTableWidgetItem(line[c]);
                this->ui->tableWidget->setItem(r, c, item);
            }
        }
    });
}
