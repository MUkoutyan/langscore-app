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

enum ScriptTableCol : int {
    EnableCheck = 0,
    ScriptName,
    TextPoint,
    Original,
    NumCols
};


namespace 
{
    bool isJapaneseLanguage(QChar ch) {
        UErrorCode status = U_ZERO_ERROR;
        UScriptCode script = uscript_getScript(ch.unicode(), &status);

        if(U_SUCCESS(status))
        {
            switch(script)
            {
            case USCRIPT_JAPANESE:
            case USCRIPT_KATAKANA_OR_HIRAGANA:
            case USCRIPT_KATAKANA:
            case USCRIPT_HIRAGANA:
            case USCRIPT_HAN:
                return true;
                break;
            default:
                break;
            }
        }
        return false;
    }

    QString getFileName(QTableWidgetItem* item) {
        assert(item->column() == ScriptTableCol::ScriptName);
        return item->data(Qt::UserRole).toString();
    }

    QString getScriptName(QTableWidgetItem* item) {
        assert(item->column() == ScriptTableCol::ScriptName);
        return item->text();
    }

}


MainCSVTable::MainCSVTable(ComponentBase* component, std::weak_ptr<LoadFileManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent), loadFileManager(std::move(loadFileManager))
    , currentWordCount(0)
    , mainFileName(new QLabel("Test", this)), mainFileWordCount(new QLabel("Test2", this))
    , tableWidget(new QTableWidget(this))
{


    this->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    auto* hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(this->mainFileName);
    hLayout->addWidget(this->mainFileWordCount);
    hLayout->addStretch(1);

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

    QFile jsonFile(tempFolder + "/Scripts.lsjson");

    if(false == jsonFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open JSON file:" << jsonFile.fileName();
        return;
    }

    QByteArray jsonData = jsonFile.readAll();
    jsonFile.close();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonData);
    if(jsonDoc.isNull() || !jsonDoc.isArray()) {
        qWarning() << "Invalid JSON format in file:" << jsonFile.fileName();
        return;
    }

    struct ScriptTextData {
        QString text;
        TextPosition pos;
    };

    QJsonArray jsonArray = jsonDoc.array();
    auto writedScripts = [&jsonArray]()
    {
        std::vector<ScriptTextData> result;
        for(const QJsonValue& value : jsonArray)
        {
            TextPosition pos;
            QJsonObject obj = value.toObject();
            pos.scriptFileName = obj["file"].toString();
            pos.type = TextPosition::Type::RowCol;
            TextPosition::RowCol rowCol;
            rowCol.row = obj["row"].toInteger();
            rowCol.col = obj["col"].toInteger();
            pos.d = rowCol;

            ScriptTextData data = {
                obj["original"].toString(),
                pos
            };

            result.emplace_back(std::move(data));
        }
        return result;
    }();

    if(writedScripts.empty()) {
        this->tableWidget->blockSignals(false);
        return;
    }

    using ScriptLineInfo = std::tuple<QString, size_t, size_t>;

    const auto& scriptList = this->setting->writeObj.scriptInfo;
    const auto IsIgnoreText = [&scriptList](const TextPosition& info)
        {
            auto fileName = info.scriptFileName;
            fileName = withoutExtension(std::move(fileName));
            if(std::holds_alternative<TextPosition::RowCol>(info.d))
            {
                auto& cell = std::get<TextPosition::RowCol>(info.d);
                auto cellPos = std::pair<size_t, size_t>{cell.row, cell.col};
                auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x) {
                    return withoutExtension(x.name) == fileName && std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), cellPos) != x.ignorePoint.cend();
                    });
                return result != scriptList.cend();
            }
            else
            {
                auto& cell = std::get<TextPosition::ScriptArg>(info.d);
                auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x) {
                    return withoutExtension(x.name) == fileName && std::find(x.ignorePoint.cbegin(), x.ignorePoint.cend(), cell.valueName) != x.ignorePoint.cend();
                    });
                return result != scriptList.cend();
            }
        };

    const auto IsIgnoreScript = [&scriptList](QString fileName) {
        fileName = withoutExtension(std::move(fileName));
        auto result = std::find_if(scriptList.cbegin(), scriptList.cend(), [&](const auto& x) {
            return withoutExtension(x.name) == fileName && x.ignore;
            });
        return result != scriptList.cend();
        };

    const auto TextColor = [&](const TextPosition& lineInfo) {
        auto tableTextColorForState = this->getColorTheme().getTextColorForState();
        if(IsIgnoreScript(lineInfo.scriptFileName)) { return tableTextColorForState[Qt::Unchecked]; }
        if(IsIgnoreText(lineInfo)) { return tableTextColorForState[Qt::PartiallyChecked]; }
        return tableTextColorForState[Qt::Checked];
    };

    // 常に非表示にするスクリプト名のフィルタリング（最適化版）
    {
        std::vector<QString> hideScriptNameList = {"langscore_custom", "langscore"};

        // スクリプト名とその行位置を一時的にマッピング
        std::unordered_map<QString, std::vector<size_t>> scriptNameToRowMap;

        // すべての行を走査して一度だけマッピングを作成
        for(size_t i = 1; i < writedScripts.size(); ++i)
        {

            //auto scriptName = getScriptName(writedScripts[i].pos.scriptFileName);
            auto scriptName = writedScripts[i].pos.scriptFileName;

            if(scriptNameToRowMap.find(scriptName) == scriptNameToRowMap.end()) {
                scriptNameToRowMap[scriptName] = std::vector<size_t>{i};
            }
            else {
                scriptNameToRowMap[scriptName].push_back(i);
            }
        }

        // 削除対象となる行番号を収集
        std::vector<size_t> rowsToRemove;
        for(const auto& hideName : hideScriptNameList) {
            auto it = scriptNameToRowMap.find(hideName);
            if(it != scriptNameToRowMap.end()) {
                // 該当スクリプト名のすべての行を削除リストに追加
                for(auto rowIndex : it->second) {
                    rowsToRemove.push_back(rowIndex);
                }
            }
        }

        // 行番号を降順にソートして削除（後ろから削除していくことでインデックスのずれを防ぐ）
        if(rowsToRemove.empty() == false) {
            std::sort(rowsToRemove.begin(), rowsToRemove.end(), std::greater<size_t>());

            for(auto rowIndex : rowsToRemove) {
                writedScripts.erase(writedScripts.begin() + rowIndex);
            }
        }
    }

    //ヘッダーを除外するので-1
    const auto numTableItems = writedScripts.size() - 1;
    QTableWidget* targetTable = this->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // デフォルトの最小高さを設定して、内容が少ない場合でも最低限の高さを確保
    targetTable->verticalHeader()->setMinimumSectionSize(28);
    targetTable->setRowCount(numTableItems);
    targetTable->setColumnCount(ScriptTableCol::NumCols);

    targetTable->setHorizontalHeaderLabels(QStringList() << "Include" << "ScriptName" << "TextPoint" << "Original Text");

    int currentScriptWordCount = 0;
    const auto& scriptExt = GetScriptExtension(this->setting->projectType);
    for(int r = 0; r < numTableItems; ++r)
    {
        auto& scriptData = writedScripts[r + 1];
        const auto& originalText = scriptData.text;
        auto& lineParsedResult = scriptData.pos;

        const auto& textColor = TextColor(lineParsedResult);

        //チェックボックス
        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
        targetTable->setItem(r, ScriptTableCol::EnableCheck, checkItem);

        const auto scriptTableItemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        //名前
        {
            auto* item = new QTableWidgetItem(originalText);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::Original, item);

            checkItem->setData(Qt::CheckStateRole, IsIgnoreText(lineParsedResult) ? Qt::Unchecked : Qt::Checked);
        }

        //チェックセル以外を初期化
        if(lineParsedResult.scriptFileName.contains(scriptExt)) {
            lineParsedResult.scriptFileName.chop(scriptExt.length());
        }
        currentScriptWordCount += wordCountUTF8(originalText);

        {
            //auto scriptName = getScriptName(lineParsedResult.scriptFileName);
            auto scriptName = lineParsedResult.scriptFileName;
            auto* item = new QTableWidgetItem(scriptName);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            item->setData(Qt::UserRole, withoutExtension(lineParsedResult.scriptFileName));
            targetTable->setItem(r, ScriptTableCol::ScriptName, item);
        }
        //テキストの位置

        if(std::holds_alternative<TextPosition::RowCol>(lineParsedResult.d))
        {
            auto& cell = std::get<TextPosition::RowCol>(lineParsedResult.d);
            auto* item = new QTableWidgetItem(QString("%1:%2").arg(cell.row).arg(cell.col));
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);
        }
        else {
            auto& cell = std::get<TextPosition::ScriptArg>(lineParsedResult.d);
            auto* item = new QTableWidgetItem(cell.valueName);
            item->setForeground(textColor);
            item->setFlags(scriptTableItemFlags);
            targetTable->setItem(r, ScriptTableCol::TextPoint, item);

        }
    }

    targetTable->resizeColumnsToContents();
    targetTable->update();

    this->tableWidget->blockSignals(false);

    //this->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));

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

void MainCSVTable::updateWordCount(QString text, Qt::CheckState state)
{
    auto wordCount = wordCountUTF8(text);
    currentWordCount += (state == Qt::Unchecked) ? -wordCount : wordCount;
    this->mainFileWordCount->setText(tr("All Word Count : %1").arg(currentWordCount));
}

QString MainCSVTable::getScriptFileNameFromTable(int row)
{
    if(auto scriptNameItem = this->scriptTableItem(row, ScriptTableCol::ScriptName)) {
        return ::getFileName(scriptNameItem);
    }
    return "";
}

// --- slot実装 ---
void MainCSVTable::onTableScrollToRow(const QString& scriptFileName)
{
    auto targetTable = this->tableWidget;
    auto rows = targetTable->rowCount();
    for(int i = 0; i < rows; ++i) {
        if(auto scriptNameItem = this->scriptTableItem(i, ScriptTableCol::ScriptName)) {
            auto scriptFile = ::getFileName(scriptNameItem);
            if(scriptFile.contains(scriptFileName)) {
                targetTable->scrollToItem(scriptNameItem, QAbstractItemView::ScrollHint::PositionAtCenter);
                break;
            }
        }
    }
}

void MainCSVTable::onTableSelectRow(const QString& scriptFileName)
{
    auto targetTable = this->tableWidget;
    auto rows = targetTable->rowCount();
    for(int i = 0; i < rows; ++i) {
        if(auto scriptNameItem = this->scriptTableItem(i, ScriptTableCol::ScriptName)) {
            auto scriptFile = ::getFileName(scriptNameItem);
            if(scriptFile.contains(scriptFileName)) {
                targetTable->selectRow(i);
                break;
            }
        }
    }
}

void MainCSVTable::onTableSelected()
{
    auto selectItems = this->tableWidget->selectedItems();
    if(selectItems.isEmpty()) { return; }
    auto item = selectItems[0];
    int row = item->row();

    auto scriptNameItem = this->scriptTableItem(row, ScriptTableCol::ScriptName);
    auto textPointItem = this->scriptTableItem(row, ScriptTableCol::TextPoint);
    if(scriptNameItem == nullptr || textPointItem == nullptr) { return; }

    auto [textRow, textCol] = ::parseScriptWithRowCol(textPointItem->text());

    //スクリプトのスクロール
    int textLen = 0;
    if(auto i = this->scriptTableItem(row, ScriptTableCol::Original)) {
        auto text = i->text();
        textLen = text.length();
    }

    auto fileName = ::getFileName(scriptNameItem);
    auto scriptName = ::getScriptName(scriptNameItem);

    this->mainFileName->setText(scriptName + "(" + fileName + ")");
    emit this->scriptTableSelected(scriptName, fileName, textRow, textCol, textLen);
}

QTableWidgetItem* MainCSVTable::scriptTableItem(int row, int col) {
    if(this->tableWidget->rowCount() <= row) { return nullptr; }
    return this->tableWidget->item(row, col);
}

void MainCSVTable::scriptTableItemChanged(QTableWidgetItem* item)
{
    if(item->column() != 0) { return; }

    auto selectItems = this->tableWidget->selectedItems();
    //選択アイテムでは無いアイテムをクリックした場合、クリック側のみを処理する。
    QSet<int> rows;
    for(auto* _item : selectItems) { rows.insert(_item->row()); }
    if(rows.contains(item->row()) == false) {
        rows.clear();
        rows.insert(item->row());
    }

    //無視リストの操作
    auto check = item->checkState();
    bool ignore = check == Qt::Unchecked;
    std::vector<QUndoCommand*> result;
    for(int row : rows)
    {
        if(auto checkItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck)) {
            result.emplace_back(new TableUndo(this, checkItem, check, ignore ? Qt::Checked : Qt::Unchecked));
        }
    }

    if(result.empty()) { return; }
    //if(_suspendHistory){
    //    discardCommand(std::move(result));
    //    return;
    //}

    const QSignalBlocker blocker(this->tableWidget);
    if(result.size() == 1) {
        this->history->push(result[0]);
    }
    else
    {
        this->history->beginMacro(tr("Change Script Table Check"));
        for(auto* command : result) {
            this->history->push(command);
        }
        this->history->endMacro();
    }
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

    //this->mainFileName->setText(treeItemName);

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

    //this->mainFileWordCount->setText(tr("Word Count : %1").arg(wordCount));

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

void MainCSVTable::setScriptFileName(QString fileName)
{
    this->mainFileName->setText(fileName);
}

void MainCSVTable::TableUndo::setValue(ValueType value) {
    //this->parent->setScriptTableItemCheck(this->target, value ? Qt::Checked : Qt::Unchecked);
}

void MainCSVTable::TableUndo::undo() {
    this->setValue(oldValue);
    if(auto textItem = this->parent->tableWidget->item(this->target->row(), ScriptTableCol::Original)) {
        this->setText(tr("Change Table State : %1").arg(textItem->text()));
    }
    else {
        this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    }
}

void MainCSVTable::TableUndo::redo() {
    this->setValue(newValue);
    if(auto textItem = this->parent->tableWidget->item(this->target->row(), ScriptTableCol::Original)) {
        this->setText(tr("Change Table State : %1").arg(textItem->text()));
    }
    else {
        this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    }
}

void MainCSVTable::changeScriptTableItemCheck(QString scriptName, Qt::CheckState check)
{
    // ここでテーブルのチェック状態や色を更新する処理を実装
    // 例: setScriptState(scriptName, check);
}
