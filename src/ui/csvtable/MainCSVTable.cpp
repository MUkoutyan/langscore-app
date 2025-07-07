#include "MainCSVTable.h"

#include <QHeaderView>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QActionGroup>
#include <QTimer>
#include <icu.h>
#include "../utility.hpp"
#include "../csv.hpp"
#include "../graphics.hpp"
#include "CSVEditDataManager.h"
#include "MainCSVTableModel.h"

using namespace langscore;

MainCSVTable::MainCSVTable(ComponentBase* component, std::weak_ptr<CSVEditDataManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent), loadFileManager(std::move(loadFileManager))
    , mainFileName(new QLabel(this)), mainFileWordCount(new QLabel(this))
    , csvEditor(new CSVEditor(this))
{

    auto* vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0); 

    auto* hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(this->mainFileName);
    hLayout->addStretch(1);
    hLayout->addWidget(this->mainFileWordCount);

    vLayout->addLayout(hLayout);
    vLayout->addWidget(this->csvEditor, 1);
    this->setLayout(vLayout);

    // 親ウィジェットを作成してレイアウトを設定
    //auto* parentWidget = this->parentWidget();
    //if (parentWidget) {
    //    parentWidget->setLayout(vLayout);
    //}
}

void MainCSVTable::clear()
{
    this->csvEditor->blockSignals(true);
    
    // 現在のモデルをクリア
    if (auto* mainModel = qobject_cast<MainCSVTableModel*>(this->csvEditor->model())) {
        mainModel->clearAll();
    }
    
    this->csvEditor->blockSignals(false);

    this->mainFileName->setText("");
    this->mainFileWordCount->setText("");
}

void MainCSVTable::setupTable() 
{
    this->csvEditor->blockSignals(true);
    
    // 現在のモデルをクリア
    if (auto* mainModel = qobject_cast<MainCSVTableModel*>(this->csvEditor->model())) {
        mainModel->clearAll();
    }
    
    const auto tempFolder = this->setting->analyzeDirectoryPath();
    this->csvEditor->blockSignals(false);
}

void MainCSVTable::setTableItemTextColor(int row, QBrush color)
{
    // CSVEditDataManagerを使用する場合、モデル側で色情報を管理することを推奨
    // 必要に応じて実装
}

// --- slot実装 ---

QModelIndex MainCSVTable::scriptTableItem(int row, int col) {
    auto* currentModel = this->csvEditor->model();
    if(!currentModel || currentModel->rowCount() <= row) { 
        return QModelIndex(); 
    }
    return currentModel->index(row, col);
}

void MainCSVTable::scriptTableItemChanged(QModelIndex item)
{
    // 必要に応じて実装
    // CSVEditDataManagerでdirtyフラグを管理
    if (auto manager = loadFileManager.lock()) {
        // 現在編集中のファイルパスを取得してdirtyフラグを設定
        // ここでは簡略化のため省略
    }
}

void MainCSVTable::showMainFileText(QString treeItemName, QString fileName)
{
    auto manager = loadFileManager.lock();
    if (!manager) {
        return;
    }

    
    // CSVEditDataManagerからMainCSVTableModelを取得
    const auto editFolder = this->setting->langscoreProjectDirectory + "/editing";
    const auto editedData = editFolder + "/" + withoutExtension(fileName) + ".csv";
    auto* mainModel = manager->getMainCSVModel(editedData);
    if(mainModel == nullptr) {
        return;
    }

    if(QFile::exists(editedData) == false) 
    {
        // JSONファイルから読み込み
        const auto translateFolder = this->setting->analyzeDirectoryPath();
        const auto filePath = translateFolder + "/" + withoutExtension(fileName) + ".lsjson";
        mainModel->loadFromJsonFile(filePath);
    }
    else {
        mainModel->loadFromFile(editedData);
    }
    
    // テーブルビューにモデルを設定
    this->csvEditor->setModel(mainModel);
    
    // 言語列を追加
    size_t index = 1;
    for(auto langs = this->setting->languages; auto& lang : langs) {
        if(lang.enable == false) { continue; }
        mainModel->insertColumn(index, lang.languageName);
        index++;
    }

    this->csvEditor->resizeRowsToContents();

    // ファイル名とワードカウントを更新
    this->mainFileName->setText(treeItemName);
    
    QTimer::singleShot(0, this, [this]() {
        this->csvEditor->setUpdatesEnabled(false);
        this->csvEditor->resizeRowsToContents();
        this->csvEditor->setUpdatesEnabled(true);
        this->csvEditor->update();
    });
}

void MainCSVTable::TableUndo::setValue(ValueType value) 
{
    if (!this->target.isValid()) {
        return;
    }
    
    // 現在のモデルを取得してsetDataメソッドを使用
    auto* currentModel = this->parent->csvEditor->model();
    if (currentModel) {
        currentModel->setData(this->target, value, Qt::EditRole);
    }
}

void MainCSVTable::TableUndo::undo() {
    this->setValue(oldValue);
    
    // undo操作の説明テキストを設定
    if (this->target.isValid()) {
        auto* currentModel = this->parent->csvEditor->model();
        if (currentModel) {
            auto displayValue = currentModel->data(this->target, Qt::DisplayRole);
            if (!displayValue.toString().isEmpty()) {
                this->setText(tr("Change Table State : %1").arg(displayValue.toString()));
            } else {
                this->setText(tr("Change Table State : Row %1").arg(this->target.row()));
            }
        }
    } else {
        this->setText(tr("Change Table State"));
    }
}

void MainCSVTable::TableUndo::redo() {
    this->setValue(newValue);
    
    // redo操作の説明テキストを設定
    if (this->target.isValid()) {
        auto* currentModel = this->parent->csvEditor->model();
        if (currentModel) {
            auto displayValue = currentModel->data(this->target, Qt::DisplayRole);
            if (!displayValue.toString().isEmpty()) {
                this->setText(tr("Change Table State : %1").arg(displayValue.toString()));
            } else {
                this->setText(tr("Change Table State : Row %1").arg(this->target.row()));
            }
        }
    } else {
        this->setText(tr("Change Table State"));
    }
}

void MainCSVTable::changeScriptTableItemCheck(QString scriptName, Qt::CheckState check)
{
    // 必要に応じて実装
}
