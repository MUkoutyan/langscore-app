#include "ScriptCSVTable.h"

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

#include <ranges>
#include "MainCSVTable.h"

using namespace langscore;

enum ScriptTableCol : int {
    EnableCheck = 0,
    ScriptName,
    TextPoint,
    Text,
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


ScriptCSVTable::ScriptCSVTable(ComponentBase* component, std::weak_ptr<CSVEditDataManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent), loadFileManager(std::move(loadFileManager))
    , showAllScriptContents(true)
    , currentScriptWordCount(0)
    , scriptFileName(new QLabel("Test", this)), scriptFileWordCount(new QLabel("Test2", this))
    , autoCheckButton(new QToolButton(this)), scriptFilterButton(new QToolButton(this))
    , tableWidget(new QTableWidget(this))
{

    this->autoCheckButton->setToolTip("Tips autoCheckButton");

    this->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* vLayout = new QVBoxLayout(this);
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    auto* hLayout = new QHBoxLayout(this);
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(this->scriptFileName);
    hLayout->addWidget(this->scriptFileWordCount);
    hLayout->addStretch(1);
    hLayout->addWidget(autoCheckButton);
    hLayout->addWidget(scriptFilterButton);

    vLayout->addLayout(hLayout);
    vLayout->addWidget(this->tableWidget, 1);

    this->setLayout(vLayout);


    {
        auto icon = this->scriptFilterButton->icon();
        auto image = icon.pixmap(this->scriptFilterButton->size()).toImage();
        graphics::ReverceHSVValue(image);
        this->scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));
    }


    //全ての内容を表示
    auto showAllContents = new QAction(tr("Show All Contents"));
    this->scriptFilterButton->addAction(showAllContents);
    showAllContents->setCheckable(true);
    showAllContents->setChecked(true);
    //無視する内容を非表示
    auto hideIgnoreContents = new QAction(tr("Hide Ignore Contents"));
    this->scriptFilterButton->addAction(hideIgnoreContents);
    hideIgnoreContents->setCheckable(true);
    hideIgnoreContents->setChecked(false);

    auto actionGroup = new QActionGroup(this);
    actionGroup->setExclusive(true);
    actionGroup->addAction(showAllContents);
    actionGroup->addAction(hideIgnoreContents);

    connect(showAllContents, &QAction::triggered, this, [this]() {
        this->showAllScriptContents = true;
        this->setupScriptTable();
    });
    connect(hideIgnoreContents, &QAction::triggered, this, [this]() {
        this->showAllScriptContents = false;
        this->setupScriptTable();
    });

    connect(this->tableWidget, &QTableWidget::itemChanged, this, &ScriptCSVTable::scriptTableItemChanged);

    connect(this->scriptFilterButton, &QToolButton::clicked, this->scriptFilterButton, &QToolButton::showMenu);


    connect(this->autoCheckButton, &QToolButton::clicked, this->autoCheckButton, &QToolButton::showMenu);
    {
        //記号のみの文のチェックを外す
        auto uncheckSignOnly = new QAction(tr("Uncheck Sign Only Text"));
        this->autoCheckButton->addAction(uncheckSignOnly);

        connect(uncheckSignOnly, &QAction::triggered, this, [this]() {
            this->unckeckSignOnlyText();
        });

        //プログラミングで使用される文のチェックを外す
        // auto uncheckProgrammingText = new QAction(tr("Uncheck Text used in programming"));
        // this->autoCheckButton->addAction(uncheckProgrammingText);
        // connect(uncheckProgrammingText, &QAction::triggered, this, [this]()
        // {
        //     auto numRow = this->tableWidget_script->rowCount();
        //     std::vector<QString> ignoreTextList = {
        //         "BigInt", "String", "Boolean", "Number", "Symbol"
        //     };
        //     for(int r=0; r<numRow; ++r)
        //     {
        //         auto text = this->tableWidget_script->item(r, ScriptTableCol::Original)->text();
        //         bool is_find = false;
        //         for(QChar qc : text) {
        //             int c = qc.toLatin1();
        //             if(c == 0){
        //                 c = qc.unicode();
        //                 if(c == 0){ continue; }
        //             }
        //             if(c < 127) {
        //                 is_find |= (std::isalpha(c) != 0);
        //             }
        //             else {
        //                 is_find = true;
        //             }
        //         }
        //         if(is_find == false){
        //             this->tableWidget_script->item(r, ScriptTableCol::EnableCheck)->setCheckState(Qt::Unchecked);
        //         }
        //     }
        // });

        //日本語を含まない文章のチェックを外す
        auto uncheckNotIncludeHiragana = new QAction(tr("Uncheck text that does not contain hiragana"));
        this->autoCheckButton->addAction(uncheckNotIncludeHiragana);
        connect(uncheckNotIncludeHiragana, &QAction::triggered, this, [this]() {
            this->uncheckNotContainHiragana();
        });
    }


}

void ScriptCSVTable::clear()
{
    this->tableWidget->blockSignals(true);
    this->tableWidget->clear();
    this->tableWidget->setRowCount(0);
    this->tableWidget->blockSignals(false);

    this->scriptFileName->setText("");
    this->scriptFileWordCount->setText("");
}

void ScriptCSVTable::setupScriptTable() 
{
    this->tableWidget->blockSignals(true);
    this->tableWidget->clear();
    const auto tempFolder = this->setting->analyzeDirectoryPath();

    auto scripts = this->setting->getScriptFileList();

    if(scripts.empty()) {
        this->tableWidget->blockSignals(false);
        return;
    }

    {
        auto rm_result = std::ranges::remove_if(scripts, [](const auto& data) {
            return settings::isLangscoreScript(data.scriptName);
        });
        scripts.erase(rm_result.begin(), rm_result.end());
    }

    using ScriptLineInfo = std::tuple<QString, size_t, size_t>;

    const auto IsIgnoreText = [this](QString fileName, const TextPosition& info)
    {
        const auto& scriptInfo = this->setting->fetchScriptInfo(fileName);
        auto result = std::ranges::find_if(scriptInfo.lines, [&](const auto& line) {
            return line == info;
        });
        if(result != scriptInfo.lines.end()) {
            return result->ignore;
        }
        return false;
    };

    const auto IsIgnoreScript = [this](QString fileName) {
        return this->setting->fetchScriptInfo(fileName).isIgnore();
    };

    const auto TextColor = [&](QString fileName, const TextPosition& lineInfo) {
        auto tableTextColorForState = this->getColorTheme().getTextColorForState();
        if(IsIgnoreScript(fileName)) { return tableTextColorForState[Qt::Unchecked]; }
        if(IsIgnoreText(fileName, lineInfo)) { return tableTextColorForState[Qt::PartiallyChecked]; }
        return tableTextColorForState[Qt::Checked];
    };

    if(showAllScriptContents == false)
    {
        {
            auto rm_result = std::ranges::remove_if(scripts, [](const auto& data) {
                if(data.scriptName.isEmpty()) { return false; }
                if(data.isIgnore()) { return true; }
                return false;
            });
            scripts.erase(rm_result.begin(), scripts.end());
        }
        {
            for(auto& script : scripts) {
                auto rm_result = std::ranges::remove_if(script.lines, [](const auto& data) {
                    return data.ignore;
                });
                script.lines.erase(rm_result.begin(), script.lines.end());
            }
        }
    }

    // 常に非表示にするスクリプト名のフィルタリング（最適化版）
    //{
    //    std::vector<QString> hideScriptNameList = {"langscore_custom", "langscore"};


    //    // すべての行を走査して一度だけマッピングを作成
    //    size_t rowCount = 0;
    //    for(auto [index, info] : scripts | std::views::enumerate)
    //    {
    //        auto scriptName = info.scriptName;
    //        // スクリプト名が非表示リストに含まれている場合はスキップ
    //        if(std::ranges::find(hideScriptNameList, scriptName) != hideScriptNameList.end()) {
    //            continue;
    //        }

    //        auto rmed_view = std::views::filter([](const auto& l) { return l.ignore == false; });
    //        for (auto&& line : info.lines | rmed_view) {
    //            scriptNameToRowMap[scriptName].push_back(rowCount++);
    //        }
    //    }

    //    // 削除対象となる行番号を収集
    //    std::vector<size_t> rowsToRemove;
    //    for(const auto& hideName : hideScriptNameList) {
    //        auto it = scriptNameToRowMap.find(hideName);
    //        if(it != scriptNameToRowMap.end()) {
    //            // 該当スクリプト名のすべての行を削除リストに追加
    //            for(auto rowIndex : it->second) {
    //                rowsToRemove.push_back(rowIndex);
    //            }
    //        }
    //    }

    //    // 行番号を降順にソートして削除（後ろから削除していくことでインデックスのずれを防ぐ）
    //    if(rowsToRemove.empty() == false) {
    //        std::ranges::sort(rowsToRemove, std::greater<size_t>{});

    //        for(auto rowIndex : rowsToRemove) {
    //            writedScripts.erase(writedScripts.begin() + rowIndex);
    //        }
    //    }
    //}

    //ヘッダーを除外するので-1
    auto totalNumTableItems = 0;
    for(const auto& script : scripts)
    {
        //無視するスクリプトは除外
        if(script.isIgnore()) { continue; }
        //無視する行は除外
        auto rmed_view = std::views::filter([](const auto& l) { return l.ignore == false; });
        totalNumTableItems += std::ranges::distance(script.lines | rmed_view);
    }
    QTableWidget* targetTable = this->tableWidget;
    targetTable->clear();
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    // デフォルトの最小高さを設定して、内容が少ない場合でも最低限の高さを確保
    targetTable->verticalHeader()->setMinimumSectionSize(28);
    targetTable->setRowCount(totalNumTableItems);
    targetTable->setColumnCount(ScriptTableCol::NumCols);

    targetTable->setHorizontalHeaderLabels(QStringList() << "Include" << "ScriptName" << "TextPoint" << "Text" << "Original Text");

    int currentScriptWordCount = 0;
    const auto& scriptExt = GetScriptExtension(this->setting->projectType);
    int currentRow = 0;
    for(const auto& scriptData : scripts)
    {
        //チェックセル以外を初期化
        auto scriptName = scriptData.scriptName;
        if(scriptName.contains(scriptExt)) {
            scriptName.chop(scriptExt.length());
        }

        const auto& lineData = scriptData.lines;
        for(auto& line : lineData)
        {
            if(line.ignore) { continue; }

            const auto& originalText = line.value;

            const auto& textColor = TextColor(scriptData.fileName, line);

            //チェックボックス
            auto* checkItem = new QTableWidgetItem();
            checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
            targetTable->setItem(currentRow, ScriptTableCol::EnableCheck, checkItem);

            const auto scriptTableItemFlags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
            //名前
            {
                auto* item = new QTableWidgetItem(originalText);
                item->setForeground(textColor);
                item->setFlags(scriptTableItemFlags);
                targetTable->setItem(currentRow, ScriptTableCol::Original, item);

                checkItem->setData(Qt::CheckStateRole, IsIgnoreText(scriptData.fileName, line) ? Qt::Unchecked : Qt::Checked);
            }

            currentScriptWordCount += wordCountUTF8(originalText);

            {
                //auto scriptName = getScriptName(lineParsedResult.scriptFileName);
                auto scriptName = scriptData.scriptName;
                auto* item = new QTableWidgetItem(scriptName);
                item->setForeground(textColor);
                item->setFlags(scriptTableItemFlags);
                item->setData(Qt::UserRole, withoutExtension(scriptData.fileName));
                targetTable->setItem(currentRow, ScriptTableCol::ScriptName, item);
                if(scriptNameToTableIndexMap.find(scriptName) == scriptNameToTableIndexMap.end()) {
                    scriptNameToTableIndexMap.emplace(scriptName, std::vector<int>{int(currentRow)});
                }
                else {
                    scriptNameToTableIndexMap[scriptName].emplace_back(currentRow);
                }
            }
            //テキストの位置

            if(std::holds_alternative<TextPosition::RowCol>(line.d))
            {
                auto& cell = std::get<TextPosition::RowCol>(line.d);
                auto* item = new QTableWidgetItem(QString("%1:%2").arg(cell.row).arg(cell.col));
                if(cell.row == 0 && cell.col == 0) {
                    item->setText("Parameter");
                }
                item->setForeground(textColor);
                item->setFlags(scriptTableItemFlags);
                targetTable->setItem(currentRow, ScriptTableCol::TextPoint, item);
            }
            else {
                auto& cell = std::get<TextPosition::ScriptArg>(line.d);
                auto* item = new QTableWidgetItem(cell.valueName);
                item->setForeground(textColor);
                item->setFlags(scriptTableItemFlags);
                targetTable->setItem(currentRow, ScriptTableCol::TextPoint, item);

            }

            {
                auto* item = new QTableWidgetItem(line.value);
                item->setForeground(textColor);
                item->setFlags(scriptTableItemFlags);
                targetTable->setItem(currentRow, ScriptTableCol::Text, item);
            }

            currentRow++;

        }
    }

    targetTable->resizeColumnsToContents();
    targetTable->update();

    this->tableWidget->blockSignals(false);

    //this->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));

}

void ScriptCSVTable::filterScriptTextData(std::vector<ScriptTextData>& scriptDataList) {
    // ScriptCSVTable::setupScriptCsvText()のフィルタ部分を移植
    // ...省略...
}

void ScriptCSVTable::setTableItemTextColor(int row, QBrush color)
{
    const auto tableCol = this->tableWidget->columnCount();
    for(int c = 0; c < tableCol; ++c) {
        if(auto i = this->scriptTableItem(row, c)) {
            i->setForeground(color);
        }
    }
}

void ScriptCSVTable::updateTableTextColor()
{
    this->tableWidget->blockSignals(true);
    auto rows = this->tableWidget->rowCount();
    auto tableTextColorForState = this->getColorTheme().getTextColorForState();
    for(int r = 0; r < rows; ++r)
    {
        if(auto checkItem = this->tableWidget->item(r, ScriptTableCol::EnableCheck)) {
            setTableItemTextColor(r, tableTextColorForState[checkItem->checkState()]);
        }
    }

    {
        auto icon = this->scriptFilterButton->icon();
        auto image = icon.pixmap(this->scriptFilterButton->size()).toImage();
        if(this->getColorTheme().getCurrentTheme() == ColorTheme::Dark) {
            graphics::ReverceHSVValue(image);
        }
        this->scriptFilterButton->setIcon(QIcon(QPixmap::fromImage(image)));
        this->scriptFilterButton->update();
    }

    this->tableWidget->blockSignals(false);
    this->tableWidget->update();
}

void ScriptCSVTable::updateScriptWordCount(QString text, Qt::CheckState state)
{
    auto wordCount = wordCountUTF8(text);
    currentScriptWordCount += (state == Qt::Unchecked) ? -wordCount : wordCount;
    this->scriptFileWordCount->setText(tr("All Script Word Count : %1").arg(currentScriptWordCount));
}

QString ScriptCSVTable::getScriptFileNameFromTable(int row)
{
    if(auto scriptNameItem = this->scriptTableItem(row, ScriptTableCol::ScriptName)) {
        return ::getFileName(scriptNameItem);
    }
    return "";
}

std::vector<int> ScriptCSVTable::fetchScriptTableSameFileRows(QString scriptName)
{
    std::vector<int> result;
    if(scriptNameToTableIndexMap.find(scriptName) != scriptNameToTableIndexMap.end()) {
        result = scriptNameToTableIndexMap[scriptName];
    }
    return result;
}

// --- slot実装 ---
void ScriptCSVTable::onScriptTableScrollToRow(const QString& scriptFileName)
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

void ScriptCSVTable::onScriptTableSelectRow(const QString& scriptFileName)
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

void ScriptCSVTable::onScriptTableSelected()
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

    this->scriptFileName->setText(scriptName + "(" + fileName + ")");
    emit this->scriptTableSelected(scriptName, fileName, textRow, textCol, textLen);
}

void ScriptCSVTable::writeToIgnoreScriptLine(int row, bool ignore)
{
    auto textPosItem = this->scriptTableItem(row, ScriptTableCol::TextPoint);
    if(textPosItem == nullptr) { return; }
    TextPosition textPos = parseScriptNameWithRowCol(textPosItem->text());
    auto fileName = getScriptFileNameFromTable(row);
    if(fileName.isEmpty()) { return; }
    //MainComponentのinvokeAnalyzeと重複
    //コンフィグの内容を変更
    if(ignore)
    {
        auto& info = setting->fetchScriptInfo(fileName);
        auto find_result = std::ranges::find_if(info.lines, [&](const auto& t) { 
            return t.type == textPos.type && t.d == textPos.d; 
        });
        if(find_result != info.lines.end()) {
            find_result->ignore = true;
        }
    }
    else {
        setting->removeScriptInfoPoint(fileName, textPos);
    }

    //ファイルツリーへ通知
    emit this->notifyScriptTableChangeItemCheck(fileName, ignore ? Qt::Unchecked : Qt::Checked);
}

QTableWidgetItem* ScriptCSVTable::scriptTableItem(int row, int col) {
    if(this->tableWidget->rowCount() <= row) { return nullptr; }
    return this->tableWidget->item(row, col);
}


void ScriptCSVTable::updateScriptIgnoreState(QString scriptName, Qt::CheckState check)
{
    //テーブル側への反映
    this->tableWidget->blockSignals(true);
    auto targetItemRow = fetchScriptTableSameFileRows(scriptName);
    auto tableTextColorForState = this->getColorTheme().getTextColorForState();
    const auto& textColor = tableTextColorForState[check];

    for(auto r : targetItemRow) 
    {
        if(auto item = this->scriptTableItem(r, ScriptTableCol::EnableCheck)){
            item->setCheckState(check);
        }
    }
    this->tableWidget->blockSignals(false);

    this->tableWidget->update();
}

void ScriptCSVTable::setScriptTableItemCheck(QTableWidgetItem* item, Qt::CheckState check)
{
    //無視リストの操作
    const int row = item->row();
    {
        const QSignalBlocker scriptTableBlocker(this->tableWidget);
        auto baseCheckItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck);
        if(baseCheckItem)
        {
            writeToIgnoreScriptLine(row, check == Qt::Unchecked);
            baseCheckItem->setCheckState(check);
        }
    }

    auto scriptItem = this->scriptTableItem(row, ScriptTableCol::ScriptName);
    QString scriptName = scriptItem ? scriptItem->text() : "";

    this->updateTableTextColor();
    emit this->notifyScriptTableChangeItemCheck(scriptName, check);


}

void ScriptCSVTable::unckeckSignOnlyText()
{
    auto numRow = this->tableWidget->rowCount();
    for(int r = 0; r < numRow; ++r)
    {
        auto text = this->tableWidget->item(r, ScriptTableCol::Original)->text();
        bool is_find = false;
        for(QChar qc : text) {
            int c = qc.toLatin1();
            if(c == 0) {
                c = qc.unicode();
                if(c == 0) { continue; }
            }
            if(0 <= c && c < 127) {
                is_find |= (std::isalpha(c) != 0);
            }
            else {
                is_find = true;
            }
        }
        if(is_find == false) {
            this->tableWidget->item(r, ScriptTableCol::EnableCheck)->setCheckState(Qt::Unchecked);
        }
    }
}

void ScriptCSVTable::uncheckNotContainHiragana()
{
    auto numRow = this->tableWidget->rowCount();
    for(int r = 0; r < numRow; ++r)
    {
        auto text = this->tableWidget->item(r, ScriptTableCol::Original)->text();
        bool is_find = false;
        for(QChar qc : text) {
            is_find |= ::isJapaneseLanguage(qc);
            if(is_find) { break; }
        }
        if(is_find == false) {
            this->tableWidget->item(r, ScriptTableCol::EnableCheck)->setCheckState(Qt::Unchecked);
        }
    }
}



void ScriptCSVTable::scriptTableItemChanged(QTableWidgetItem* item)
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

Qt::CheckState ScriptCSVTable::getTreeCheckStateBasedOnTable(QString scriptName)
{
    //同じスクリプトが全て無視されたかをチェック
    int enableCheckCount = 0;
    auto rows = fetchScriptTableSameFileRows(scriptName);
    for(auto row : rows)
    {
        if(auto checkItem = this->scriptTableItem(row, ScriptTableCol::EnableCheck))
        {
            if(checkItem->checkState() == Qt::Checked) {
                enableCheckCount++;
            }
        }
    }

    auto treeCheckState = Qt::Checked;
    if(rows.size() != enableCheckCount) {
        treeCheckState = (enableCheckCount == 0) ? Qt::Unchecked : Qt::PartiallyChecked;
    }

    return treeCheckState;
}

void ScriptCSVTable::showNormalJsonText(QString treeItemName, QString fileName)
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

void ScriptCSVTable::setScriptFileName(QString fileName)
{
    this->scriptFileName->setText(fileName);
}

void MainCSVTable::receive(DispatchType type, const QVariantList& args)
{
    if(type == DispatchType::SaveProject)
    {
        auto editingDir = this->setting->langscoreProjectDirectory + "/editing";

    }
}

void ScriptCSVTable::TableUndo::setValue(ValueType value) {
    this->parent->setScriptTableItemCheck(this->target, value ? Qt::Checked : Qt::Unchecked);
}

void ScriptCSVTable::TableUndo::undo() {
    this->setValue(oldValue);
    if(auto textItem = this->parent->tableWidget->item(this->target->row(), ScriptTableCol::Original)) {
        this->setText(tr("Change Table State : %1").arg(textItem->text()));
    }
    else {
        this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    }
}

void ScriptCSVTable::TableUndo::redo() {
    this->setValue(newValue);
    if(auto textItem = this->parent->tableWidget->item(this->target->row(), ScriptTableCol::Original)) {
        this->setText(tr("Change Table State : %1").arg(textItem->text()));
    }
    else {
        this->setText(tr("Change Table State : Row %1").arg(this->target->row()));
    }
}

void ScriptCSVTable::changeScriptTableItemCheck(QString scriptName, Qt::CheckState check)
{
    updateScriptIgnoreState(scriptName, check);
    updateTableTextColor();
}
