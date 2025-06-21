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

using namespace langscore;


MainCSVTable::MainCSVTable(ComponentBase* component, std::weak_ptr<LoadFileManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent), loadFileManager(std::move(loadFileManager))
    , mainFileName(new QLabel(this)), mainFileWordCount(new QLabel(this))
    , tableWidget(new QTableWidget(this))
{
    this->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    auto* hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(this->mainFileName);
    hLayout->addStretch(1);
    hLayout->addWidget(this->mainFileWordCount);

    vLayout->addLayout(hLayout);
    vLayout->addWidget(this->tableWidget, 1);

    this->setLayout(vLayout);
}

void MainCSVTable::clear()
{
    this->tableWidget->blockSignals(true);
    this->tableWidget->clear();
    this->tableWidget->setRowCount(0);
    this->tableWidget->blockSignals(false);

    this->mainFileName->setText("");
    this->mainFileWordCount->setText("");
}

void MainCSVTable::setupTable() 
{
    this->tableWidget->blockSignals(true);
    this->tableWidget->clear();
    const auto tempFolder = this->setting->analyzeDirectoryPath();
    this->tableWidget->blockSignals(false);

}

void MainCSVTable::setTableItemTextColor(int row, QBrush color)
{
    const auto tableCol = this->tableWidget->columnCount();
    for(int c = 0; c < tableCol; ++c) {
        if(auto i = this->scriptTableItem(row, c)) {
            i->setForeground(color);
        }
    }
}

// --- slot実装 ---


QTableWidgetItem* MainCSVTable::scriptTableItem(int row, int col) {
    if(this->tableWidget->rowCount() <= row) { return nullptr; }
    return this->tableWidget->item(row, col);
}

void MainCSVTable::scriptTableItemChanged(QTableWidgetItem* item)
{
}

void MainCSVTable::showMainFileText(QString treeItemName, QString fileName)
{
    const auto translateFolder = this->setting->analyzeDirectoryPath();
    QFile file(translateFolder + "/" + withoutExtension(fileName) + ".lsjson");
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file:" << file.fileName();
        return;
    }

    QByteArray jsonData = file.readAll();
    file.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << file.fileName();
        return;
    }

    QJsonArray jsonArray = jsonDoc.array();
    if(jsonArray.isEmpty()) {
        qWarning() << "JSON array is empty in file:" << file.fileName();
        return;
    }

    this->mainFileName->setText(treeItemName);

    // テーブルの準備
    QTableWidget* targetTable = this->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->setRowCount(jsonArray.size());
    targetTable->setColumnCount(2); // "original" と "type" の2列


    // ヘッダー設定
    targetTable->setHorizontalHeaderLabels(QStringList() << tr("Original") << tr("Type"));

    size_t wordCount = 0;
    for(int r = 0; r < jsonArray.size(); ++r) {
        QJsonObject item = jsonArray.at(r).toObject();

        // Original テキスト
        QString originalText = item["original"].toString();
        wordCount += wordCountUTF8(originalText);
        auto* originalItem = new QTableWidgetItem(originalText);
        targetTable->setItem(r, 0, originalItem);

        // Type (配列の場合があるので結合して表示)
        QJsonArray typeArray = item["type"].toArray();
        QStringList typeList;
        for(const QJsonValue& typeValue : typeArray) {
            typeList.append(typeValue.toString());
        }
        QString typeText = typeList.join(", ");
        auto* typeItem = new QTableWidgetItem(typeText);
        targetTable->setItem(r, 1, typeItem);
    }

    this->mainFileWordCount->setText(tr("Word Count : %1").arg(wordCount));

    // 強制的にテーブルを更新して行の高さを調整
    targetTable->resizeRowsToContents();

    // 遅延処理でサイズの再調整を行う
    QTimer::singleShot(0, this, [targetTable]() {
        targetTable->setUpdatesEnabled(false);
        targetTable->resizeRowsToContents();
        targetTable->setUpdatesEnabled(true);
        targetTable->update();
    });
}

void MainCSVTable::TableUndo::setValue(ValueType value) {
    //this->parent->setScriptTableItemCheck(this->target, value ? Qt::Checked : Qt::Unchecked);
}

void MainCSVTable::TableUndo::undo() {
    //this->setValue(oldValue);
    //if(auto textItem = this->parent->tableWidget->item(this->target->row(), ScriptTableCol::Original)) {
    //    this->setText(tr("Change Table State : %1").arg(textItem->text()));
    //}
    //else {
    //    this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    //}
}

void MainCSVTable::TableUndo::redo() {
    //this->setValue(newValue);
    //if(auto textItem = this->parent->tableWidget->item(this->target->row(), ScriptTableCol::Original)) {
    //    this->setText(tr("Change Table State : %1").arg(textItem->text()));
    //}
    //else {
    //    this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    //}
}

void MainCSVTable::changeScriptTableItemCheck(QString scriptName, Qt::CheckState check)
{
    // ここでテーブルのチェック状態や色を更新する処理を実装
    // 例: setScriptState(scriptName, check);
}
