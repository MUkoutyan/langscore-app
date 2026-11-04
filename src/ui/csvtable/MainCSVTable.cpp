#include "MainCSVTable.h"

#include <QHeaderView>
#include <QFile>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QActionGroup>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QCheckBox>
#include <QShortcut>
#include <QHash>
#include <algorithm>
#include <icu.h>
#include "../utility.hpp"
#include "../csv.hpp"
#include "../graphics.hpp"
#include "../invoker.h"
#include "EditDataManager.h"
#include "MainCSVTableModel.h"
#include "service/LanguageNames.h"

using namespace langscore;

enum TableItemType {
    CSVRow = Qt::UserRole + 1,
    LanguageIndex
};

MainCSVTable::MainCSVTable(ComponentBase* component, std::weak_ptr<EditDataManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent)
    , loadFileManager(loadFileManager)
    , mainFileName(new QLabel(this))
    , mainFileWordCount(new QLabel(this))
    , searchEdit(new QLineEdit(this))
    , hideTranslatedCheck(new QCheckBox(tr("Hide Translated Rows"), this))
    , languageFontCheck(new QCheckBox(tr("Apply language font"), this))
    , fixedWidthCheck(new QCheckBox(tr("Fixed cell width"), this))
    , fixedWidthSpinBox(new QSpinBox(this))
, hideLanguageColumnsAction(new QPushButton(tr("Filter Columns")))
    , validateButton(new QPushButton(tr("Validate")))
    , validateResultListButton(new QPushButton(tr("Show validate result")))
    , validateResultDialog(new QDialog(this))
    , validateResultTable(new QTableWidget(this))
    , settingButton(new QToolButton(this))
    , settingPane(new QWidget(this))
    , currentModel(nullptr)
    , csvEditor(new CSVEditor(this->loadFileManager, this, this))
    , cellErrorLog(new QTextEdit(this))
    , _invoker(new invoker(this))
    , updateTimer(nullptr)
    , _finishInvoke(false)
    , _showAllScriptContents(false)
{
    this->cellErrorLog->setReadOnly(true);
    this->cellErrorLog->setMaximumHeight(60);

    auto appSettings = ComponentBase::getAppSettings();

    //設定ポップアップ（列・フォント・幅など、表示の細かい設定をまとめる）
    {
        settingPane->setWindowFlags(Qt::Popup);
        settingPane->setVisible(false);

        auto layout = new QVBoxLayout(settingPane);
        layout->addWidget(hideLanguageColumnsAction);
        layout->addWidget(languageFontCheck);

        fixedWidthSpinBox->setRange(10, 2000);
        fixedWidthSpinBox->setSuffix(" px");
        auto fixedWidthLayout = new QHBoxLayout();
        fixedWidthLayout->addWidget(fixedWidthCheck);
        fixedWidthLayout->addWidget(fixedWidthSpinBox);
        layout->addLayout(fixedWidthLayout);

        //前回の設定を戻す
        languageFontCheck->setChecked(appSettings.value("MainCSVTable/useLanguageFont", false).toBool());
        fixedWidthCheck->setChecked(appSettings.value("MainCSVTable/fixedCellWidth", false).toBool());
        fixedWidthSpinBox->setValue(appSettings.value("MainCSVTable/cellWidth", 100).toInt());
        fixedWidthSpinBox->setEnabled(fixedWidthCheck->isChecked());

        connect(languageFontCheck, &QCheckBox::toggled, this, [this](bool checked)
        {
            ComponentBase::getAppSettings().setValue("MainCSVTable/useLanguageFont", checked);
            if(this->currentModel == nullptr) { return; }
            this->currentModel->setUseLanguageFont(checked);
            if(currentModel->rowCount() == 0 || currentModel->columnCount() == 0) { return; }
            auto top = currentModel->index(0, 0);
            auto bottom = currentModel->index(currentModel->rowCount() - 1, currentModel->columnCount() - 1);
            emit currentModel->dataChanged(top, bottom, QVector<int>{Qt::FontRole});
        });

        connect(fixedWidthCheck, &QCheckBox::toggled, this, [this](bool checked)
        {
            ComponentBase::getAppSettings().setValue("MainCSVTable/fixedCellWidth", checked);
            this->fixedWidthSpinBox->setEnabled(checked);
            this->restoreColumnWidths();
        });

        connect(fixedWidthSpinBox, &QSpinBox::valueChanged, this, [this](int value)
        {
            ComponentBase::getAppSettings().setValue("MainCSVTable/cellWidth", value);
            this->restoreColumnWidths();
        });

        connect(settingButton, &QToolButton::clicked, this, [this](bool) {
            this->settingPane->setVisible(true);
            this->settingPane->move(QCursor::pos());
        });
    }

    {
        auto layout = new QVBoxLayout();
        layout->addWidget(this->validateResultTable);
        validateResultDialog->setLayout(layout);
    }

    this->validateButton->setEnabled(false);
    this->validateResultListButton->setEnabled(false);
    this->validateResultListButton->setToolTip(tr("F8: Next error  Shift+F8: Previous error"));

    this->validateResultTable->setSelectionBehavior(QTableWidget::SelectionBehavior::SelectRows);

    //検索
    searchEdit->setPlaceholderText(tr("Search"));
    searchEdit->setClearButtonEnabled(true);
    connect(searchEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        if(this->_proxyModel) {
            this->_proxyModel->setFilterText(text);
        }
    });
    auto* searchShortcut = new QShortcut(QKeySequence::Find, this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        this->searchEdit->setFocus();
        this->searchEdit->selectAll();
    });

    //検証結果のセルへ移動
    auto* nextErrorShortcut = new QShortcut(QKeySequence(Qt::Key_F8), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(nextErrorShortcut, &QShortcut::activated, this, [this]() { this->selectNextError(true); });
    auto* previousErrorShortcut = new QShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F8), this, nullptr, nullptr, Qt::WidgetWithChildrenShortcut);
    connect(previousErrorShortcut, &QShortcut::activated, this, [this]() { this->selectNextError(false); });

    //翻訳済みの行を隠す
    _showAllScriptContents = appSettings.value("MainCSVTable/showAllContents", true).toBool();
    hideTranslatedCheck->setChecked(_showAllScriptContents == false);
    connect(hideTranslatedCheck, &QCheckBox::toggled, this, [this](bool checked)
    {
        _showAllScriptContents = (checked == false);
        ComponentBase::getAppSettings().setValue("MainCSVTable/showAllContents", _showAllScriptContents);
        if(this->_proxyModel) {
            this->_proxyModel->setFilterNoTranslateColumn(checked);
        }
    });

    auto* vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    auto* hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(this->mainFileName);
    hLayout->addSpacing(10);
    hLayout->addWidget(this->searchEdit, 1);
    hLayout->addWidget(this->hideTranslatedCheck);
    hLayout->addStretch(1);
    hLayout->addWidget(this->mainFileWordCount);
    hLayout->addSpacing(20);
    hLayout->addWidget(this->validateButton);
    hLayout->addWidget(this->validateResultListButton);

    this->settingButton->setIcon(QIcon(":/images/resources/image/settings.png"));
    this->settingButton->setToolTip(tr("Display settings"));
    hLayout->addWidget(this->settingButton);

    vLayout->addLayout(hLayout);
    vLayout->addWidget(this->csvEditor, 2);
    vLayout->addWidget(this->cellErrorLog, 0);
    this->cellErrorLog->setVisible(false);
    this->setLayout(vLayout);
    
    connect(hideLanguageColumnsAction, &QPushButton::clicked, this, [this]()
    {
        this->csvEditor->hideLanguageColumns();
    });

    connect(validateButton, &QPushButton::clicked, this, [this]()
    {
        if(this->currentModel == nullptr) { return; }
        this->currentModel->saveToFile(this->currentFileName);
        //終わるまで押せないようにする（NotifyFinishValidateCSV で戻す）
        this->validateButton->setEnabled(false);
        this->dispatch(DispatchType::ValidateCSV, {this->currentFileName});
    });

    connect(validateResultListButton, &QPushButton::clicked, this, [this]()
    {
        this->validateResultDialog->show();
        this->validateResultDialog->raise();
    });

    connect(this->validateResultTable, &QTableWidget::itemSelectionChanged, this, [this]() 
    {
        auto selectItems = this->validateResultTable->selectedItems();
        if(selectItems.empty()) { return; }

        auto item = selectItems[0];
        
        bool isOk = false;
        auto csvRow = item->data(TableItemType::CSVRow).toInt(&isOk) - 1;
        if(isOk == false || csvRow < 0) {
            return;
        }

        isOk = false;
        auto csvCol = item->data(TableItemType::LanguageIndex).toInt(&isOk);
        if(isOk == false) {
            return;
        }
        this->selectCell(csvRow, csvCol);
    });
}

//モデルの行・列のセルを選択し、見える位置までスクロールする
void MainCSVTable::selectCell(int row, int column)
{
    if(this->currentModel == nullptr) { return; }
    if(row < 0 || this->currentModel->rowCount() <= row) { return; }
    if(column < 0 || this->currentModel->columnCount() <= column) { return; }

    auto sourceIdx = this->currentModel->index(row, column);
    QModelIndex selectIdx = _proxyModel ? _proxyModel->mapFromSource(sourceIdx) : QModelIndex(sourceIdx);
    if(selectIdx.isValid()) {
        this->csvEditor->selectionModel()->select(selectIdx, QItemSelectionModel::ClearAndSelect);
        this->csvEditor->scrollTo(selectIdx, QTableWidget::ScrollHint::PositionAtCenter);
    }
}

//表示中のファイルの検証結果（ValidationErrorInfo::id）のセルを選択する
void MainCSVTable::selectError(quint64 errorId)
{
    if(this->currentModel == nullptr) { return; }
    const auto& errors = this->runtimeData->errors;
    const auto fileErrors = errors.find(this->currentModel->getCurrentShowFileName());
    if(fileErrors == errors.end()) { return; }
    const auto info = std::ranges::find_if(fileErrors->second, [errorId](const auto& x) { return x.id == errorId; });
    //行の無い、ファイル全体の結果は移動しない
    if(info == fileErrors->second.end() || info->row < 1) { return; }

    //info.row はヘッダーを 0 行目としたCSVの行。言語が複数あるときは最初の言語の列にする。言語が無ければ原文の列
    const auto row = static_cast<int>(info->row - 1);
    const auto language = info->language.split(" ").value(0);
    int column = 0;
    for(int c = 0; c < this->currentModel->columnCount(); ++c) {
        if(this->currentModel->headerData(c, Qt::Horizontal, Qt::UserRole).toString() == language) {
            column = c;
            break;
        }
    }
    //表を開いた直後は行の高さの調整が後で行われるため、その後にスクロールする
    QTimer::singleShot(0, this, [this, row, column]() { this->selectCell(row, column); });
}

void MainCSVTable::selectNextError(bool forward)
{
    if(this->currentModel == nullptr || this->_proxyModel == nullptr) { return; }
    const auto& errors = this->runtimeData->errors;
    const auto fileErrors = errors.find(this->currentModel->getCurrentShowFileName());
    if(fileErrors == errors.end()) { return; }

    //言語名から列を参照できるようにする
    QHash<QString, int> columnOfLanguage;
    for(int column = 0; column < this->currentModel->columnCount(); ++column) {
        columnOfLanguage.insert(this->currentModel->headerData(column, Qt::Horizontal, Qt::UserRole).toString(), column);
    }

    //検証結果のセルを、表示上の位置（行, 列）で並べる。隠れている行・列は飛ばす
    std::vector<QModelIndex> errorCells;
    for(const auto& info : fileErrors->second)
    {
        if(info.row < 1 || info.summary == ValidationErrorInfo::IncludeCR) { continue; }
        const auto column = columnOfLanguage.value(info.language.split(" ").value(0), 0);
        const auto proxyIndex = this->_proxyModel->mapFromSource(this->currentModel->index(static_cast<int>(info.row - 1), column));
        if(proxyIndex.isValid()) {
            errorCells.push_back(proxyIndex);
        }
    }
    if(errorCells.empty()) { return; }
    const auto byPosition = [](const QModelIndex& a, const QModelIndex& b) {
        return std::pair(a.row(), a.column()) < std::pair(b.row(), b.column());
    };
    std::ranges::sort(errorCells, byPosition);

    //今のセルより後かつ最初のものへ。端まで来たら反対側に戻る
    const auto current = this->csvEditor->currentIndex();
    QModelIndex target;
    if(forward)
    {
        const auto found = std::ranges::find_if(errorCells, [&](const QModelIndex& x) { return current.isValid() == false || byPosition(current, x); });
        target = (found != errorCells.end()) ? *found : errorCells.front();
    }
    else
    {
        const auto found = std::find_if(errorCells.rbegin(), errorCells.rend(), [&](const QModelIndex& x) { return current.isValid() == false || byPosition(x, current); });
        target = (found != errorCells.rend()) ? *found : errorCells.back();
    }
    this->csvEditor->selectionModel()->setCurrentIndex(target, QItemSelectionModel::ClearAndSelect);
    this->csvEditor->scrollTo(target, QTableView::ScrollHint::PositionAtCenter);
}

//ファイル名の後ろに、保存していない変更があることを示す「*」を付ける
void MainCSVTable::updateFileNameLabel()
{
    const bool modified = (this->currentModel != nullptr && this->currentModel->isModified());
    this->mainFileName->setText(modified ? this->currentTreeItemName + " *" : this->currentTreeItemName);
    this->mainFileName->setToolTip(modified ? tr("There are unsaved changes.") : QString());
}

void MainCSVTable::updateProgressLabel()
{
    if(this->currentModel == nullptr)
    {
        this->mainFileWordCount->setText("");
        return;
    }
    const auto [translatedRows, totalRows] = this->currentModel->translatedRowCount();
    const auto percent = (0 < totalRows) ? translatedRows * 100 / totalRows : 0;
    this->mainFileWordCount->setText(tr("Translated: %1 / %2 (%3%)").arg(translatedRows).arg(totalRows).arg(percent));
    emit this->fileProgressChanged(this->currentTreeFileName, translatedRows, totalRows);
}

void MainCSVTable::clear()
{
    this->csvEditor->blockSignals(true);

    //モデルは EditDataManager が所有しているので、参照を外すだけにする
    if(_proxyModel) {
        _proxyModel->setSourceModel(nullptr);
    }
    for(const auto& connection : this->modelConnections) {
        disconnect(connection);
    }
    this->modelConnections.clear();
    currentModel = nullptr;
    this->validateButton->setEnabled(false);

    this->csvEditor->blockSignals(false);

    this->currentTreeItemName.clear();
    this->currentTreeFileName.clear();
this->mainFileName->setText("");
    this->mainFileWordCount->setText("");
    this->currentFileName.clear();
}

//ファイルのモデルを返す。まだ読み込んでいなければ読み込む（EditDataManager がファイルごとに保持する）
MainCSVTableModel* MainCSVTable::loadModel(const QString& fileName)
{
    auto manager = loadFileManager.lock();
    if (!manager) {
        return nullptr;
    }

    const auto editedData = this->setting->langscoreProjectDirectory + "/editing/" + withoutExtension(fileName) + ".csv";
    auto* mainModel = manager->getMainCSVModel(editedData);
    if(mainModel == nullptr) {
        return nullptr;
    }
    //proxyでsettings->IsLanguageColumnを使うため、事前に設定する
    mainModel->setSettings(this->setting);
    mainModel->setRuntimeData(this->runtimeData);

    //読み込み済みのモデルを読み直すと未保存の編集が失われるため、未読み込みの場合のみ読み込む。
    if(mainModel->columnCount() == 0) {
        this->fillModelFromFiles(*mainModel, fileName);
    }
    return mainModel;
}

void MainCSVTable::fillModelFromFiles(langscore::CSVEditorTableModel& model, const QString& fileName) const
{
    const auto editedData = this->setting->langscoreProjectDirectory + "/editing/" + withoutExtension(fileName) + ".csv";

    //行は常に最新の解析結果から作る
    const auto translateFolder = this->setting->analyzeDirectoryPath();
    const bool loadedFromAnalyzeResult = model.loadFromJsonFile(translateFolder + "/" + withoutExtension(fileName) + ".lsjson");
    if(loadedFromAnalyzeResult == false && QFile::exists(editedData)) {
        model.loadFromEditCSVFile(editedData);
    }

    size_t index = 1;
    for(auto langs = this->setting->languages; auto& lang : langs) {
        if(lang.enable == false) { continue; }
        model.insertColumn(index, lang.languageName);
        index++;
    }

    //editing側に保存した翻訳を適用する
    if(loadedFromAnalyzeResult && QFile::exists(editedData)) {
        model.mergeTranslationsFromCsv(editedData);
    }
}

//ファイルの進捗（すべての言語が埋まった行の数, 全体の行数）
std::pair<int, int> MainCSVTable::fileProgress(const QString& fileName)
{
    const auto editedData = this->setting->langscoreProjectDirectory + "/editing/" + withoutExtension(fileName) + ".csv";
    if(auto manager = this->loadFileManager.lock())
    {
        //開いたことのあるファイルは、未保存の編集を含めて数える
        auto* openedModel = manager->findMainCSVModel(editedData);
        if(openedModel != nullptr && openedModel->columnCount() != 0) {
            return openedModel->translatedRowCount();
        }
    }
    //開いていないファイルは、保存先に影響しないよう一時的なモデルで数える
    langscore::CSVEditorTableModel model;
    model.setSettings(this->setting);
    this->fillModelFromFiles(model, fileName);
    return model.translatedRowCount();
}

void MainCSVTable::notifyAllFileProgress()
{
    //ponytail: 起動時に全ファイルを読むため、ファイル数が非常に多いと遅くなる。問題になれば別スレッドで計算する
    const auto analyzeFolder = this->setting->analyzeDirectoryPath();
    const auto notifyFiles = [this, &analyzeFolder](const auto& infoList) {
        for(const auto& info : infoList)
        {
            if(QFile::exists(analyzeFolder + "/" + withoutExtension(info.fileName) + ".lsjson") == false) { continue; }
            const auto [translatedRows, totalRows] = this->fileProgress(info.fileName);
            emit this->fileProgressChanged(info.fileName, translatedRows, totalRows);
        }
    };
    notifyFiles(this->setting->writeObj.basicDataInfo);
    notifyFiles(this->setting->writeObj.mapDataInfo);
}

//全検証の前に呼ぶ。divisiはediting/にあるCSVだけを検証するため、
//まだ開いていないファイルも読み込んでおき、保存でediting/に書かれるようにする
void MainCSVTable::loadAllMainFiles()
{
    const auto analyzeFolder = this->setting->analyzeDirectoryPath();
    const auto loadFiles = [this, &analyzeFolder](const auto& infoList) {
        for(const auto& info : infoList) {
            if(info.ignore) { continue; }
            //解析結果が無いファイルは、原文の無いCSVになるため作らない
            if(QFile::exists(analyzeFolder + "/" + withoutExtension(info.fileName) + ".lsjson") == false) { continue; }
            this->loadModel(info.fileName);
        }
    };
    loadFiles(this->setting->writeObj.basicDataInfo);
    loadFiles(this->setting->writeObj.mapDataInfo);
}

void MainCSVTable::showMainFileText(QString treeItemName, QString fileName)
{
    auto* mainModel = this->loadModel(fileName);
    for(const auto& connection : this->modelConnections) {
        disconnect(connection);
    }
    this->modelConnections.clear();
    this->currentModel = mainModel;
    this->currentTreeItemName = treeItemName;
    this->currentTreeFileName = fileName;
if(mainModel == nullptr) {
        this->validateButton->setEnabled(false);
        this->updateFileNameLabel();
        this->updateProgressLabel();
        return;
    }
    mainModel->setUseLanguageFont(this->languageFontCheck->isChecked());

    //未保存の印と進捗を、編集や保存に合わせて更新する
    this->modelConnections.append(connect(mainModel, &MainCSVTableModel::modifiedChanged, this, &MainCSVTable::updateFileNameLabel));
    this->modelConnections.append(connect(mainModel, &QAbstractItemModel::dataChanged, this, &MainCSVTable::updateProgressLabel));
    this->modelConnections.append(connect(mainModel, &QAbstractItemModel::modelReset, this, &MainCSVTable::updateProgressLabel));

    this->csvEditor->horizontalHeader()->blockSignals(true);

    const auto editedData = this->setting->langscoreProjectDirectory + "/editing/" + withoutExtension(fileName) + ".csv";
    this->currentFileName = editedData;
    //検証結果はファイルのベース名をキーにしている（ValidationService / FileTree）
    const auto& errors = this->runtimeData->errors;
    if(errors.empty() || errors.find(QFileInfo(editedData).baseName()) == errors.end()) {
        this->validateResultListButton->setEnabled(false);
    }
    else {
        this->validateResultListButton->setEnabled(true);
    }
    
    // プロキシモデルを初期化し、テーブルビューに設定
    const bool isNewProxy = (_proxyModel == nullptr);
    if(isNewProxy) {
        _proxyModel = new CSVEditorSortFilterProxyModel(this);
        //入力した行が絞り込みや並べ替えで編集中に消えたり動いたりしないよう、条件を変えたときだけ反映する
        _proxyModel->setDynamicSortFilter(false);
        connect(_proxyModel, &CSVEditorSortFilterProxyModel::columnVisibilityChanged, this, &MainCSVTable::restoreColumnWidths);
    }
    _proxyModel->setSortState(0, SortOrder::None);
    _proxyModel->setSourceModel(mainModel);
    _proxyModel->setFilterNoTranslateColumn(_showAllScriptContents == false);
    _proxyModel->setFilterText(this->searchEdit->text());
    _proxyModel->setSettingsGroup(CSVEditorSortFilterProxyModel::settingsGroupForProject(this->setting->langscoreProjectDirectory));
    this->csvEditor->setModel(_proxyModel);

    _proxyModel->loadColumnFilterFromSettings();


    // ファイル名と進捗を更新
    this->updateFileNameLabel();
    this->updateProgressLabel();
    this->validateButton->setEnabled(true);
    
    QTimer::singleShot(0, this, [this]() {
        this->csvEditor->setUpdatesEnabled(false);
        this->csvEditor->resizeRowsToContents();
        this->restoreColumnWidths();
        this->csvEditor->setUpdatesEnabled(true);
        this->csvEditor->update();
    });


    //同じ proxy を使い回している間は選択モデルも同じなので、接続はファイルを開くたびではなく一度だけ行う
    if(isNewProxy == false) {
        this->csvEditor->horizontalHeader()->blockSignals(false);
        return;
    }
    auto selectionModel = this->csvEditor->selectionModel();
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection& selected, const QItemSelection& deselected)
    {
        if(currentModel == nullptr) { return; }
        Q_UNUSED(deselected);
        if(selected.indexes().isEmpty()) {
            this->cellErrorLog->setVisible(false);
            return;
        }
        auto proxyIndex = selected.indexes().first();
        auto sourceIndex = _proxyModel ? _proxyModel->mapToSource(proxyIndex) : proxyIndex;

        auto currentShowFileName = currentModel->getCurrentShowFileName();
        const auto& errors = this->runtimeData->errors;
        if(errors.find(currentShowFileName) == errors.end()) { return; }

        const auto& errorList = errors.at(currentShowFileName);

        auto header = currentModel->headerData(sourceIndex.column(), Qt::Orientation::Horizontal, Qt::UserRole).toString();

        QString message;
        for(const auto& error : errorList) 
        {
            if(error.summary == ValidationErrorInfo::IncludeCR) {
                continue;
            }
            const bool isFileError = error.row == 0 && error.language.isEmpty();
            const bool isCellError = error.row - 1 == sourceIndex.row() && error.language.contains(header);
            if(isFileError || isCellError) 
            {
                if(error.type == ValidationErrorInfo::Error) {
                    message += tr("[Error] ");
                }
                else if(error.type == ValidationErrorInfo::Warning) {
                    message += tr("[Warning] ");
                }

                message += error.getErrorText() + "\n";
            }
        }

        if(message.isEmpty()) {
            this->cellErrorLog->setVisible(false);
            return;
        }
        this->cellErrorLog->setText(message);
        this->cellErrorLog->setVisible(true);

    });

    this->csvEditor->horizontalHeader()->blockSignals(false);
}

void MainCSVTable::restoreColumnWidths()
{
    if(!_proxyModel || !this->setting) { return; }

    const int proxyColCount = _proxyModel->columnCount();
    auto* header = this->csvEditor->horizontalHeader();
    if(this->fixedWidthCheck->isChecked())
    {
        header->setSectionResizeMode(QHeaderView::Fixed);
        for(int proxyCol = 0; proxyCol < proxyColCount; ++proxyCol) {
            this->csvEditor->setColumnWidth(proxyCol, this->fixedWidthSpinBox->value());
        }
        return;
    }
    header->setSectionResizeMode(QHeaderView::Interactive);
    for(int proxyCol = 0; proxyCol < proxyColCount; ++proxyCol) {
        const QString header = _proxyModel->headerData(proxyCol, Qt::Horizontal, Qt::UserRole).toString();
        if(header == "original") {
            this->csvEditor->setColumnWidth(proxyCol, this->setting->originColumnWidth);
            continue;
        }
        for(const auto& lang : this->setting->languages) {
            if(lang.languageName == header) {
                this->csvEditor->setColumnWidth(proxyCol, lang.columnSize);
                break;
            }
        }
    }
}

void MainCSVTable::receive(DispatchType type, const QVariantList& args)
{
    if(type == DispatchType::SaveProject)
    {
        //保存は WriteModeComponent が行う。受け取る順番によらず印を消せるよう、保存の後に更新する
        QTimer::singleShot(0, this, &MainCSVTable::updateFileNameLabel);
    }
    else if(type == DispatchType::NotifyFinishValidateCSV)
    {
        this->validateButton->setEnabled(this->currentModel != nullptr);
        //検証の結果が新しくなったので、検証の後に編集したセルの記録を消す
        if(auto manager = this->loadFileManager.lock()) {
            manager->clearEditedSinceValidation();
        }
        const auto& errors = this->runtimeData->errors;
        if(errors.empty()) {
            this->validateResultListButton->setEnabled(false);
            return;
        }

        auto currentShowFileName = langscore::getFileNameWithoutExtension(this->currentFileName);
        if(errors.find(currentShowFileName) == errors.end()) {
            this->validateResultListButton->setEnabled(false);
            return;
        }
        this->validateResultListButton->setEnabled(true);

        const auto& errorList = errors.at(currentShowFileName);
        this->validateResultTable->clearContents();
        this->validateResultTable->setRowCount(errorList.size());
        this->validateResultTable->setColumnCount(4);
        this->validateResultTable->setHorizontalHeaderLabels(QStringList() << tr("Row") << tr("Type") << tr("Language") << tr("Description"));

        std::unordered_map<QString, int> header_col;
        if(this->currentModel == nullptr){ return; }
        auto numHeader = this->currentModel->columnCount();
        for(int i = 0; i < numHeader; ++i) {
            auto headerName = this->currentModel->headerData(i, Qt::Horizontal, Qt::UserRole).toString();
            header_col[headerName] = i;
        }
        
        int row = 0;
        for(const auto& info : errorList)
        {
            if(info.summary == ValidationErrorInfo::IncludeCR) {
                continue;
            }
            int col = 0;

            QString infoTypeText;
            QColor color;
            switch(info.type) {
            case ValidationErrorInfo::Error:
                infoTypeText = "Error";
                color = QColor(236, 11, 0, 51);
                break;
            case ValidationErrorInfo::Warning:
                infoTypeText = "Warning";
                color = QColor(240, 227, 0, 51);
                break;
            case ValidationErrorInfo::Invalid:
                infoTypeText = "Invalid";
                break;
            }
            
            const auto createItem = [&](QString text) {
                auto item = new QTableWidgetItem(text);
                item->setBackground(color);
                item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                item->setData(TableItemType::CSVRow, info.row);

                if(info.language.isEmpty()) {
                    return item;
                }

                auto langs = info.language.split(" ");
                if(langs.isEmpty()) {
                    return item;
                }
                auto find_result = header_col.find(langs[0]);
                if(find_result != header_col.end()) {
                    auto lang_index = find_result->second;
                    item->setData(TableItemType::LanguageIndex, lang_index);
                }
                return item;
            };
            {
                //info内のrowはCSVファイルの行数が格納される。
                //CSVにはヘッダーが含まれるため、実質1インデックスとなる。
                //エディターで表示する際はヘッダーはセルには存在しないため、0インデックスとなる。
                auto item = createItem((1 <= info.row) ? QString::number(info.row - 1) : tr("File"));
                this->validateResultTable->setItem(row, col++, item);
            }
            {
                auto item = createItem(infoTypeText);
                this->validateResultTable->setItem(row, col++, item);
            }
            {
                auto langs = info.language.split(" ");
                if(langs.isEmpty() == false) 
                {
                    std::ranges::transform(langs, langs.begin(), [](const auto& l) {
                        return langscore::languageDisplayName(l);
                    });

                    auto item = createItem(langs.join(", "));
                    this->validateResultTable->setItem(row, col++, item);
                }
                else {
                    auto item = createItem(info.language);
                    this->validateResultTable->setItem(row, col++, item);
                }
            }
            {
                auto item = createItem(info.getErrorText());
                this->validateResultTable->setItem(row, col++, item);
            }
            row++;
        }

        QTimer::singleShot(1, this, [this]() {
            this->validateResultTable->resizeColumnsToContents();
            this->csvEditor->update();
            // モデルの背景色やユーザーロールが更新されているため、ビューへ反映させる
            if(currentModel && currentModel->rowCount() > 0 && currentModel->columnCount() > 0) {
                auto top = currentModel->index(0, 0);
                auto bottom = currentModel->index(currentModel->rowCount() - 1, currentModel->columnCount() - 1);
                emit currentModel->dataChanged(top, bottom, QVector<int>{Qt::BackgroundRole, Qt::UserRole});
            }
            this->update();
            this->csvEditor->viewport()->update();
        });

    }
}


void MainCSVTable::TableUndo::setValue(ValueType value)
{
    if(!this->target.isValid()) {
        return;
    }

    // 現在のモデルを取得してsetDataメソッドを使用
    auto* currentModel = this->parent->csvEditor->model();
    if(currentModel) {
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
