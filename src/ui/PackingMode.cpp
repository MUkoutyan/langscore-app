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
#include <QFileDialog>
#include <QMessageBox>
#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QClipboard>

enum ErrorTextCol
{
    Type = 0,
    Summary,
    Language,
    Details,
    File,
    Row,
};

PackingMode::PackingMode(ComponentBase *settings, QWidget *parent)
    : QWidget{parent}
    , ComponentBase(settings)
    , ui(new Ui::PackingMode)
    , _invoker(new invoker(this))
    , _mutex()
    , csvTableViewModel(new PackingCSVTableViewModel())
    , clipDetectSettingTree(new ClipDetectSettingTree(this->setting, this->history, this))
    , _finishInvoke(false)
    , errorInfoIndex(0)
    , currentShowCSV("")
    , isValidate(false)
    , showLog(false)
    , suspendResizeToContents(false)
    , attentionIcon(":/images/resources/image/attention.svg")
    , warningIcon(":/images/resources/image/warning.svg")
    , treeMenu(nullptr)
{
    ui->setupUi(this);

    this->ui->invalidCsvView->hide();

    this->ui->modeDesc->hide();
    this->ui->treeWidget->setStyleSheet("QTreeView::item { height: 28px; }");
    this->ui->splitter->setStretchFactor(1, 1);

    this->ui->logText->SetSettings(this->setting);

    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);

    this->ui->verticalLayout_4->addWidget(this->clipDetectSettingTree);


    connect(this->ui->moveToWrite, &QToolButton::clicked, this, &PackingMode::toPrevPage);
    connect(this->ui->validateButton, &QPushButton::clicked, this, &PackingMode::validate);
    connect(this->ui->packingButton, &QPushButton::clicked, this, &PackingMode::packing);

    connect(this->ui->openDirectory, &QPushButton::clicked, this, [this](){
        auto path = QFileDialog::getExistingDirectory(this, tr("Select Input Directory"), this->setting->translateDirectoryPath());
        if(path.isEmpty()){ return; }
        this->setPackingSourceDir(path);
    });

    connect(this->ui->openInvalidCsvButton, &QPushButton::clicked, this, [this]() {
        if(lastSelectedInvalidCSVPath.isEmpty() == false) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(this->lastSelectedInvalidCSVPath));
        }
    });

    connect(this->ui->packingSourceDirectory, &QLineEdit::textChanged, this, &PackingMode::setPackingSourceDir);

    connect(_invoker, &invoker::getStdOut, this->ui->logText, &InvokerLogViewer::writeText);
    connect(_invoker, &invoker::getStdOut, this, &PackingMode::addErrorText);
    connect(_invoker, &invoker::finish, this, [this](int, QProcess::ExitStatus status)
    {
        if(status == QProcess::ExitStatus::CrashExit){
            this->ui->logText->writeText(tr("Crashed."));
        }
        this->ui->packingButton->setEnabled((setting->projectType == settings::VXAce));
        this->ui->moveToWrite->setEnabled(true);
        this->ui->treeWidget->blockSignals(false);
        this->ui->tableView->blockSignals(false);
        this->_finishInvoke = true;

        if(this->showLog){
            this->ui->tabWidget->setCurrentWidget(this->ui->logTab);
        }

        if(isValidate){
            if(this->ui->treeWidget->topLevelItemCount() == 0){
                this->dispatch(DispatchType::StatusMessage, {tr("Valid!"), 5000});
            }
        }
        else{
            this->ui->validateButton->setEnabled(true);
            this->dispatch(DispatchType::StatusMessage, {tr("Complete Packing!"), 5000});
        }

        // 強制的にテーブルを更新して行の高さを調整
        this->ui->tableView->resizeRowsToContents();

        // 遅延処理でサイズの再調整を行う
        QTimer::singleShot(0, this, [this]() {
            this->ui->tableView->setUpdatesEnabled(false);
            this->ui->tableView->resizeRowsToContents();
            this->ui->tableView->setUpdatesEnabled(true);
            this->ui->tableView->update();
        });
    });

    connect(this->ui->treeWidget, &QTreeWidget::itemSelectionChanged, this, [this]()
    {
        auto selectedItems = this->ui->treeWidget->selectedItems();
        if(selectedItems.empty()){ return; }
        this->ui->tableView->clearSelection();
        auto item = selectedItems[0];
        if(item->parent()){
            this->setupCsvTable(item->parent()->data(0, Qt::UserRole).toString());
            this->highlightError(item);
        }
        else{
            this->setupCsvTable(item->data(0, Qt::UserRole).toString());
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

    connect(this->csvTableViewModel, &PackingCSVTableViewModel::requestResizeContents, this, &PackingMode::resizeCsvTable);

    // 既存の QTableWidget を削除し、QTableView をセット
    this->ui->tableView->installEventFilter(this);
    this->ui->tableView->setModel(csvTableViewModel);
    this->ui->tableView->setSelectionBehavior(QAbstractItemView::SelectItems);
    this->ui->tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    this->ui->tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    this->ui->tableView->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    this->ui->tableView->setAlternatingRowColors(true);

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

void PackingMode::setupCsvTable(QString filePath)
{
    if(currentShowCSV == filePath){ return; }

    const auto SetInvalidCsvMessage = [this, &filePath](int row, QString detail)
    {
        QStringList texts;
        auto file = QFile(filePath);
        if(file.open(QIODevice::ReadOnly)) {
            texts = QString(file.readAll()).split("\n");
        }

        auto splitedDetail = detail.split("\n");
        if(splitedDetail.empty() == false) 
        {
            auto splitedDetailLineSize = splitedDetail.size();
            int lineNumber = -1;
            //テキストの行数を取得
            for(int i = 0; i < texts.size(); ++i) 
            {
                //末尾から検索
                const auto& checkText = splitedDetail.back();

                auto index = texts[i].indexOf(checkText);
                if(index == -1) { continue; }

                if(splitedDetailLineSize <= 1) {
                    lineNumber = i+1;
                    break;
                }
                else
                {
                    auto checkIndex = qMax(0, i - splitedDetailLineSize + 1);
                    if(texts[checkIndex].contains(splitedDetail[0]))
                    {
                        for(int i = 1; i < splitedDetailLineSize - 1; ++i) {
                            if(!texts[checkIndex + i].contains(splitedDetail[i])) {
                                checkIndex = -1;
                                break;
                            }
                        }
                    }
                    else {
                        checkIndex = -1;
                    }
                    if(checkIndex != -1) {
                        lineNumber = checkIndex+1;
                        break;
                    }
                }
            }

            QString text;
            if(detail.isEmpty()) {
                //%1行目(テキストなら%2行目)辺りの記述が原因かもしれません。
                text += tr("This may be due to the description around the %1 line.(line %2 for text)").arg(row).arg(lineNumber);
            }
            else {
                //%1辺りの記述が原因かもしれません。(csv:%2行目 text:%3行目)
                text += tr("The description around\n%1\nmay be the cause.(csv:%2 line, text:%3 line)").arg(detail).arg(row).arg(lineNumber);
            }
            this->ui->invalidCsvAboutText->setText(text);
        }
        this->ui->invalidCsvAboutText->show();

        this->ui->tableView->hide();
        this->ui->invalidCsvView->show();

        lastSelectedInvalidCSVPath = filePath;
        this->currentShowCSV = filePath;
    };

    if(this->errors.find(filePath) != this->errors.end()) {
        auto find_result = std::ranges::find_if(this->errors[filePath], [](const auto& x) {
            return x.type == ValidationErrorInfo::Error && x.summary == ValidationErrorInfo::InvalidCSV;
        });
        if(find_result != this->errors[filePath].end()) {
            this->ui->tableView->hide();
            this->ui->invalidCsvView->show();
            
            auto& info = *find_result;
            SetInvalidCsvMessage(info.row, info.detail);
            return;
        }
    }

    auto allRows = langscore::readCsv(filePath);
    auto _header = QStringList(allRows[0].begin(), allRows[0].end());
    allRows.erase(allRows.begin());

    //整合性のチェック
    //※末列が空白の場合でも引っかかってしまうため、一旦コメントアウト。
    //int rowCount = 0;
    //for(auto& row : allRows)
    //{
    //    if(row.size() != _header.size()) {
    //        SetInvalidCsvMessage(rowCount, row.empty() == false ? row.front() : "");
    //        return;
    //    }
    //    rowCount++;
    //}

    this->csvTableViewModel->setCsvFile(filePath, std::move(allRows), std::move(_header));

    if(this->ui->invalidCsvView->isVisible()) {
        this->ui->invalidCsvView->hide();
        this->ui->tableView->show();
    }

    _mutex.lock();
    if(errors.find(filePath) != errors.end()) {
        this->csvTableViewModel->appendErrors(errors[filePath]);
    }
    _mutex.unlock();

    auto *targetTable = this->ui->tableView;
    targetTable->setModel(csvTableViewModel);
    targetTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    targetTable->horizontalHeader()->setStretchLastSection(true);

    this->currentShowCSV = filePath;
}

void PackingMode::setupCsvTree()
{
    auto path = this->ui->packingSourceDirectory->text();
    if(path.isEmpty()) { return; }
    auto files = QDir(path).entryInfoList({"*.csv"});

    for(const auto& file : files)
    {
        auto filePath = file.absoluteFilePath();
        auto item = new QTreeWidgetItem();
        item->setData(0, Qt::UserRole, filePath);
        auto fileName = langscore::withoutExtension(file.fileName());
        item->setText(0, fileName);

        treeTopItemList[filePath] = item;
        this->ui->treeWidget->addTopLevelItem(item);
    }

}

void PackingMode::highlightError(QTreeWidgetItem *treeItem)
{
    auto id = treeItem->data(0, Qt::UserRole).toULongLong();
    if(id == 0){ return; }

    auto file = treeItem->parent()->data(0, Qt::UserRole).toString();
    const auto& errorList = errors[file];

    auto result = std::find_if(errorList.cbegin(), errorList.cend(), [id](const auto& x){
        return x.id ==id;
    });
    if(result == errorList.cend()){ return; }

    const auto numRow = this->csvTableViewModel->rowCount();
    const auto numCol = this->csvTableViewModel->columnCount();


    if(numRow < result->row) {
        this->suspendResizeToContents = true;
        while(this->csvTableViewModel->canFetchMore() && result->row - 1 >= this->csvTableViewModel->rowCount()) {
            this->csvTableViewModel->fetchMore();
        }
        this->suspendResizeToContents = false;
        this->resizeCsvTable();
    }

    for(int c = 0; c < numCol; ++c)
    {
        auto index = this->csvTableViewModel->index(result->row - 1, c);
        auto tableData = this->csvTableViewModel->data(index, Qt::UserRole);
        auto tableId = tableData.toULongLong();
        if(tableId == 0) { continue; }

        if(id == tableId) {
            this->ui->tableView->scrollTo(index, QAbstractItemView::ScrollHint::PositionAtCenter);
            return;
        }
    }

}

void PackingMode::showEvent(QShowEvent*)
{
    if(this->setting->packingInputDirectory.isEmpty()){
        this->setting->setPackingDirectory("");
    }

    this->setPackingSourceDir(this->setting->packingInputDirectory);

    if(this->setting->projectType == settings::VXAce){
        this->ui->outputFolder->setText(this->setting->gameProjectPath + "/Data/Translate");
        this->ui->packingButton->setEnabled(true);
        this->ui->packingButton->setText(QCoreApplication::translate("PackingMode", "Packing Translate File", nullptr));
    }
    else{
        this->ui->outputFolder->setText(this->setting->gameProjectPath + "/data/translate");
        this->ui->packingButton->setEnabled(false);
        //MV/MZではパッキングの必要はありません
        this->ui->packingButton->setText(tr("No packing is required for MV/MZ."));
    }
    
    this->clipDetectSettingTree->setup();
}

inline bool PackingMode::eventFilter(QObject* obj, QEvent* event)
{
    if(obj == this->ui->tableView && event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->matches(QKeySequence::Copy)) {
            copySelectedCell();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void PackingMode::copySelectedCell()
{
    QModelIndexList selectedIndexes = this->ui->tableView->selectionModel()->selectedIndexes();
    if(selectedIndexes.isEmpty()) {
        return;
    }

    QModelIndex index = selectedIndexes.first();
    QString cellText = this->csvTableViewModel->data(index, Qt::DisplayRole).toString();

    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(cellText);
}

void PackingMode::validate()
{
    this->ui->validateButton->setEnabled(false);
    this->ui->packingButton->setEnabled(false);
    this->ui->moveToWrite->setEnabled(false);
    this->ui->treeWidget->blockSignals(true);
    this->ui->tableView->blockSignals(true);

    this->ui->treeWidget->clear();
    this->treeTopItemList.clear();
    // this->ui->tableView->clear();
    // this->ui->tableView->clearContents();
    this->updateList.clear();
    this->errors.clear();

    this->currentShowCSV = "";
    this->errorInfoIndex = 0;
    this->isValidate = true;

    auto searchPackingSourceDir = this->ui->packingSourceDirectory->text();
    bool needSave = false;
    if(setting->packingInputDirectory != searchPackingSourceDir){
        //パッキング入力ディレクトリを変更したため、プロジェクトを保存する必要があります。
        //保存しますか？
        auto button = QMessageBox::question(this, tr("Confirmation of save a project."), tr("The packing input directory has changed and the project needs to be saved."), QMessageBox::Save|QMessageBox::Cancel,QMessageBox::Cancel);
        if(button == QMessageBox::Cancel){
            return;
        }

        needSave = true;
        setting->setPackingDirectory(searchPackingSourceDir);
        this->ui->treeWidget->clear();
    }

    this->setupCsvTree();

    //設定ファイル側に整合性チェック用データが何も埋められていなくても保存対象にする。
    //if(this->setting->validateObj.textInfo.empty() || needSave)
    //{
    //    needSave = true;
    //    this->setting->validateObj.csvDataList.clear();
    //    auto numTreeItems = this->ui->treeWidget->topLevelItemCount();

    //    for(int i=0; i<numTreeItems; ++i)
    //    {
    //        auto item = this->ui->treeWidget->topLevelItem(i);
    //        auto fileName = item->text(0);
    //        settings::ValidationCSVInfo data;
    //        data.name = fileName;
    //        this->setting->validateObj.csvDataList.emplace_back(std::move(data));
    //    }
    //}

    if(needSave) {
        setting->saveForProject();
    }

    this->showLog = false;
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
    this->isValidate = false;

    this->ui->logText->clear();

    this->setting->packingInputDirectory = this->ui->packingSourceDirectory->text();
    this->setting->saveForProject();

    this->showLog = true;
    _invoker->packing();
}

std::vector<ValidationErrorInfo> PackingMode::processJsonBuffer(const QString& input)
{
    std::vector<ValidationErrorInfo> errors;
    int pos = 0;
    const int len = input.length();

    // 入力全体を走査
    while(pos < len)
    {
        // JSON オブジェクトは '{' で始まると仮定
        int start = input.indexOf('{', pos);
        if(start == -1)
            break;  // '{' が見つからなければ終了

        // '{' から対応する '}' を探す
        int depth = 0;
        bool inString = false;  // ダブルクォート内かどうか
        bool escape = false;    // エスケープ中かどうか
        int end = -1;
        for(int i = start; i < len; ++i)
        {
            QChar ch = input.at(i);
            if(inString)
            {
                // 文字列内はエスケープ処理に注意
                if(escape)
                {
                    escape = false;
                }
                else
                {
                    if(ch == '\\')
                        escape = true;
                    else if(ch == '"')
                        inString = false;
                }
            }
            else
            {
                if(ch == '"')
                {
                    inString = true;
                }
                else if(ch == '{')
                {
                    ++depth;
                }
                else if(ch == '}')
                {
                    --depth;
                    if(depth == 0)
                    {
                        end = i;
                        break;
                    }
                }
            }
        }

        // 終端が見つからなければ、後続のデータを待つ必要があるので break
        if(end == -1)
            break;

        // 完全な JSON オブジェクトを抽出（trim() して不要な空白を除去）
        QString jsonStr = input.mid(start, end - start + 1).trimmed();

        // UTF-8 に変換してから QJsonDocument でパース
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
        if(parseError.error != QJsonParseError::NoError)
        {
            qWarning() << "JSON parse error:" << parseError.errorString() << "in:" << jsonStr;
        }
        else if(!doc.isObject())
        {
            qWarning() << "JSON is not an object:" << jsonStr;
        }
        else
        {
            QJsonObject obj = doc.object();
            ValidationErrorInfo info;
            info.id = (++errorInfoIndex);   //1開始にする

            if(obj.contains("Type"))
            {
                int type = obj.value("Type").toInt();
                if(ValidationErrorInfo::Error <= type && type < ValidationErrorInfo::Invalid) {
                    info.type = static_cast<ValidationErrorInfo::ErrorType>(type);
                }
                else {
                    info.type = ValidationErrorInfo::Invalid;
                }
            }

            if(obj.contains("Summary"))
            {
                int summary = obj.value("Summary").toInt();
                if(ValidationErrorInfo::None < summary && summary < ValidationErrorInfo::Max) {
                    info.summary = static_cast<ValidationErrorInfo::ErrorSummary>(summary);
                } 
                else {
                    info.summary = ValidationErrorInfo::None;
                }
            }
            if(obj.contains("File")) {
                info.filePath = obj.value("File").toString();
            }
            if(obj.contains("Row")) {
                info.row = static_cast<size_t>(obj.value("Row").toInteger());
            }
            if(obj.contains("Width")) {
                info.width = obj.value("Width").toInt();
            }
            if(obj.contains("Language")) {
                info.language = obj.value("Language").toString();
            }
            if(obj.contains("Details")) {
                info.detail = obj.value("Details").toString();
            }


            // 1件の ErrorInfo を追加
            errors.emplace_back(std::move(info));
        }

        // 次の JSON オブジェクトの探索位置を更新
        pos = end + 1;
    }

    return errors;
}

void PackingMode::addErrorText(QString text)
{
    if(text.isEmpty()){ return; }

    auto infos = processJsonBuffer(text);
    std::vector<ValidationErrorInfo> needUpdateErrorInfos;
    auto currentFileName = this->csvTableViewModel->getCurrentCsvFile();
    for(auto&& info : infos)
    {
        QFileInfo fileInfo(info.filePath);
        auto fileName = fileInfo.absoluteFilePath();
        _mutex.lock();
        if(currentFileName == fileName) {
            if(fileName == currentFileName) {
                needUpdateErrorInfos.emplace_back(info);
            }
        }
        errors[fileName].emplace_back(std::move(info));
        updateList[fileName] = true;
        _mutex.unlock();
    }

    if(needUpdateErrorInfos.empty() == false) {
        this->csvTableViewModel->appendErrors(std::move(needUpdateErrorInfos));
    }

    this->update();
}

ValidationErrorInfo PackingMode::convertErrorInfo(std::vector<QString> csvText)
{
    ValidationErrorInfo info;

    auto typeText = csvText[ErrorTextCol::Type];
    if(typeText == "Warning"){
        info.type = ValidationErrorInfo::Warning;
    }
    else if(typeText == "Error"){
        info.type = ValidationErrorInfo::Error;
    }
    else {
        info.type = ValidationErrorInfo::Invalid;
        return info;
    }

    bool isOk = false;
    auto summaryRaw = csvText[ErrorTextCol::Summary].toInt(&isOk);
    if(isOk)
    {
        if(ValidationErrorInfo::EmptyCol <= summaryRaw && summaryRaw <= ValidationErrorInfo::Max){
            info.summary = static_cast<ValidationErrorInfo::ErrorSummary>(summaryRaw);
        }
        else{
            info.summary = ValidationErrorInfo::None;
            info.type = ValidationErrorInfo::Invalid;
            return info;
        }
    }

    info.language   = csvText[ErrorTextCol::Language];
    info.detail     = csvText[ErrorTextCol::Details];
    info.row        = csvText[ErrorTextCol::Row].toULongLong();
    info.id         = (++errorInfoIndex);   //1開始にする

    return info;
}

void PackingMode::resizeCsvTable()
{
    if(suspendResizeToContents) {
        return;
    }
    this->ui->tableView->horizontalHeader()->resizeSections(QHeaderView::ResizeToContents);
    this->ui->tableView->verticalHeader()->resizeSections(QHeaderView::ResizeToContents);
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

                treeTopItemList[updateInfo.first] = item;
                this->ui->treeWidget->addTopLevelItem(item);
            }
            else {
                item = treeTopItemList[updateInfo.first];

                //アイコンの設定　エラー優先で警告は次点
                auto error = std::find_if(infoList.cbegin(), infoList.cend(), [](const auto& x){ return x.type == ValidationErrorInfo::Error; });
                if(error != infoList.cend()){
                    item->setIcon(0, attentionIcon);
                }
                else
                {
                    auto warn = std::find_if(infoList.cbegin(), infoList.cend(), [](const auto& x){ return x.type == ValidationErrorInfo::Warning; });
                    if(warn != infoList.cend()){
                        item->setIcon(0, warningIcon);
                    }
                }
            }

            auto child = new QTreeWidgetItem();
            [&](QTreeWidgetItem* item)
            {
                QString text;
                if(info.type == ValidationErrorInfo::Warning){
                    item->setIcon(0, warningIcon);
                    item->setForeground(0, QColor(240, 227, 0));
                }
                else if(info.type == ValidationErrorInfo::Error){
                    item->setIcon(0, attentionIcon);
                    item->setForeground(0, QColor(236, 11, 0));
                }


                 switch(info.summary){
                     case ValidationErrorInfo::EmptyCol:
                         text += tr(" Empty Column") + "[" + info.language + "]";
                         break;
                     case ValidationErrorInfo::NotFoundEsc:
                         text += tr(" Not Found Esc") + "[" + info.language + "] (" + info.detail + ")";
                         break;
                     case ValidationErrorInfo::UnclosedEsc:
                         text += tr(" Unclosed Esc") + "[" + info.language + "] (" + info.detail + ")";
                         break;
                     case ValidationErrorInfo::IncludeCR:
                         text += tr(" Include \"\r\n\"");
                         break;
                     case ValidationErrorInfo::NotEQLang:
                         text += tr(" The specified language does not match the language in the CSV");
                         break;
                     case ValidationErrorInfo::PartiallyClipped:
                         //この文章は一部が見切れています。
                         text += tr(" Part of this text is cut off.");
                         break;
                     case ValidationErrorInfo::FullyClipped:
                         //この文章は完全に見切れています。
                         text += tr(" This text is completely cut off.");
                         break;
                     case ValidationErrorInfo::OverTextCount:
                         //指定した文字数を超過しています。
                         text += tr(" The specified number of characters has been exceeded. (num %1)").arg(info.width);
                         break;
                     case ValidationErrorInfo::InvalidCSV:
                         if(info.detail.isEmpty()) {
                             //無効なCSVです。%1行目辺りの記述が原因かもしれません。
                             text += tr(" Invalid CSV, This may be due to the description around the %1 line.").arg(info.row);
                         }
                         else {
                             //無効なCSVです。%1行目の%2辺りの記述が原因かもしれません。
                             text += tr(" Invalid CSV. The description around %2 in the %1 row may be cause.").arg(info.row).arg(info.detail);
                         }
                         break;
                     default:
                         break;
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

        if(this->ui->treeWidget->topLevelItemCount() == 0){
            this->dispatch(DispatchType::StatusMessage, {tr("Valid!"), 5000});
        }
    }
}

void PackingMode::setPackingSourceDir(QString path)
{
    auto dir = QFileInfo(path);
    auto exists = dir.exists() && dir.isDir();
    this->ui->validateButton->setEnabled(exists);
    this->ui->packingButton->setEnabled(exists && (setting->projectType == settings::VXAce));
    this->ui->packingSourceDirectory->blockSignals(true);
    auto palette = this->ui->packingSourceDirectory->palette();
    palette.setColor(QPalette::Text, exists ? this->palette().color(QPalette::Text) : Qt::red);
    this->ui->packingSourceDirectory->setPalette(palette);
    this->ui->packingSourceDirectory->setText(path);
    this->ui->packingSourceDirectory->blockSignals(false);

    this->setupCsvTree();
}



PackingCSVTableViewModel::PackingCSVTableViewModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

PackingCSVTableViewModel::~PackingCSVTableViewModel() {
}

void PackingCSVTableViewModel::setCsvFile(const QString& filePath, std::vector<std::vector<QString>> contents, QStringList _header)
{
    beginResetModel();

    csvContents.clear();
    header.clear();
    errors.clear();
    fullCsvContents.clear();

    if(contents.empty() == false) {
        header = std::move(_header);
        fullCsvContents = std::move(contents);
    }

    endResetModel();
    currentFile = filePath;
    return;
}

int PackingCSVTableViewModel::rowCount(const QModelIndex& parent) const 
{
    if(parent.isValid()) {
        return 0;
    }
    return static_cast<int>(csvContents.size());
}

int PackingCSVTableViewModel::columnCount(const QModelIndex& parent) const 
{
    Q_UNUSED(parent);
    return header.size();
}

QVariant PackingCSVTableViewModel::data(const QModelIndex& index, int role) const 
{
    if(!index.isValid()) {
        return QVariant();
    }

    int row = index.row();
    int col = index.column();
    if(role == Qt::DisplayRole) {
        return readDataAt(row, col);
    }
    else if(role == Qt::BackgroundRole)
    {
        if(errors.empty()) {
            return QVariant();
        }
        auto find_result = std::ranges::find_if(errors, [this, row, h = this->header[col]](const auto& x) {
            return x.row-1 == row && h == x.language;
        });
        if(find_result != errors.end()) {
            if(find_result->type == ValidationErrorInfo::Error) {
                return QColor(236, 11, 0, 51);
            }
            else if(find_result->type == ValidationErrorInfo::Warning) {
                return QColor(240, 227, 0, 51);
            }
        }
    }
    else if(role == Qt::UserRole) 
    {
        if(errors.empty()) {
            return QVariant();
        }
        auto find_result = std::ranges::find_if(errors, [this, row, h = this->header[col]](const auto& x) {
            return x.row - 1 == row && h == x.language;
        });
        if(find_result != errors.end()) {
            return find_result->id;
        }
    }

    return QVariant();
}

QVariant PackingCSVTableViewModel::headerData(int section, Qt::Orientation orientation, int role) const 
{
    if(role != Qt::DisplayRole) {
        return QVariant();
    }

    if(orientation == Qt::Horizontal) {
        return section < header.size() ? header[section] : QVariant();
    }
    else {
        // 縦方向は行番号（1始まり）
        return section + 1;
    }
}

void PackingCSVTableViewModel::appendErrors(std::vector<ValidationErrorInfo> infos)
{
    for(auto&& info : infos){
        errors.emplace_back(std::move(info));
    }
   
    beginResetModel();
    endResetModel();
}

QString PackingCSVTableViewModel::readDataAt(int row, int col) const 
{
    if(static_cast<size_t>(row) >= csvContents.size()) {
        return "";
    }
    if(static_cast<size_t>(col) >= csvContents[row].size()) {
        return "";
    }
    return csvContents[row][col];
}


bool PackingCSVTableViewModel::canFetchMore(const QModelIndex& parent) const 
{
    if(parent.isValid()) {
        return false;
    }
    // 表示中の行数がCSV全体の行数に達していなければ、さらに読み込み可能
    return csvContents.size() < fullCsvContents.size();
}

void PackingCSVTableViewModel::fetchMore(const QModelIndex& parent) 
{
    if(parent.isValid()) {
        return;
    }

    int displayedRows = static_cast<int>(csvContents.size());
    int totalRows = static_cast<int>(fullCsvContents.size());
    int remainingRows = totalRows - displayedRows;
    int rowsToFetch = std::min(batchSize, remainingRows);
    if(rowsToFetch <= 0) {
        return;
    }

    //batchSize件または残り行数分だけ、csvContentsに追加
    beginInsertRows(QModelIndex(), displayedRows, displayedRows + rowsToFetch - 1);
    for(int i = 0; i < rowsToFetch; ++i) {
        csvContents.emplace_back(fullCsvContents[displayedRows + i]);
    }
    endInsertRows();

    emit this->requestResizeContents();
}