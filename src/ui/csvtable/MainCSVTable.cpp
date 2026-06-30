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
#include <icu.h>
#include "../utility.hpp"
#include "../csv.hpp"
#include "../graphics.hpp"
#include "../invoker.h"
#include "CSVEditDataManager.h"
#include "MainCSVTableModel.h"
#include "service/LanguageNames.h"

using namespace langscore;

enum TableItemType {
    CSVRow = Qt::UserRole + 1,
    LanguageIndex
};

MainCSVTable::MainCSVTable(ComponentBase* component, std::weak_ptr<CSVEditDataManager> loadFileManager, QWidget* parent)
    : ComponentBase(component), QWidget(parent)
    , loadFileManager(loadFileManager)
    , mainFileName(new QLabel(this))
    , mainFileWordCount(new QLabel(this))
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
{
    this->cellErrorLog->setReadOnly(true);
    this->cellErrorLog->setMaximumHeight(60);

    //設定ダイアログ
    {
        settingPane->setWindowFlags(Qt::Popup);
        settingPane->setVisible(false);

        auto layout = new QVBoxLayout(this);
        settingPane->setLayout(layout);
        layout->addWidget(hideLanguageColumnsAction);

        auto check = new QCheckBox("apply language font", this);
        layout->addWidget(check);

        connect(check, &QCheckBox::clicked, this, [this](bool checked) 
        {
            if(this->currentModel) 
            {
                this->currentModel->setUseLanguageFont(checked);
                auto top = currentModel->index(0, 0);
                auto bottom = currentModel->index(currentModel->rowCount() - 1, currentModel->columnCount() - 1);
                emit currentModel->dataChanged(top, bottom, QVector<int>{Qt::FontRole});
            }
            this->csvEditor->update();
        });

        // --- Fixed Cell Width Option Start ---
        auto fixedWidthLayout = new QHBoxLayout();
        auto fixedWidthCheck = new QCheckBox("Fixed Cell Width", this);
        auto widthSpinBox = new QSpinBox(this);
        widthSpinBox->setRange(10, 2000);
        widthSpinBox->setValue(100);
        widthSpinBox->setSuffix(" px");
        widthSpinBox->setEnabled(false);

        fixedWidthLayout->addWidget(fixedWidthCheck);
        fixedWidthLayout->addWidget(widthSpinBox);
        layout->addLayout(fixedWidthLayout);

        connect(fixedWidthCheck, &QCheckBox::toggled, this, [this, widthSpinBox](bool checked) {
            widthSpinBox->setEnabled(checked);
            auto header = this->csvEditor->horizontalHeader();
            if(checked) {
                header->setSectionResizeMode(QHeaderView::Fixed);
                header->setDefaultSectionSize(widthSpinBox->value());
            }
            else {
                header->setSectionResizeMode(QHeaderView::Interactive);
            }
        });

        connect(widthSpinBox, &QSpinBox::valueChanged, this, [this, fixedWidthCheck](int value) {
            if(fixedWidthCheck->isChecked()) {
                this->csvEditor->horizontalHeader()->setDefaultSectionSize(value);
            }
        });
        // --- Fixed Cell Width Option End ---

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

    this->validateResultTable->setSelectionBehavior(QTableWidget::SelectionBehavior::SelectRows);

    auto* vLayout = new QVBoxLayout();
    vLayout->setContentsMargins(0, 0, 0, 0);
    vLayout->setSpacing(0);

    auto* hLayout = new QHBoxLayout();
    hLayout->setContentsMargins(0, 0, 0, 0);
    hLayout->addWidget(this->mainFileName);
    hLayout->addStretch(1);
    hLayout->addWidget(this->validateButton);
    hLayout->addWidget(this->validateResultListButton);
    hLayout->addWidget(this->mainFileWordCount);

    this->settingButton->setIcon(QIcon(":/images/resources/image/settings.png"));
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
        this->currentModel->saveToFile(this->currentFileName);
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
        if(isOk == false || (csvCol < 0 || this->currentModel->columnCount() <= csvCol)) {
            return;
        }

        auto sourceIdx = this->currentModel->index(csvRow, csvCol);
        QModelIndex selectIdx = _proxyModel ? _proxyModel->mapFromSource(sourceIdx) : QModelIndex(sourceIdx);
        if(selectIdx.isValid()) {
            this->csvEditor->selectionModel()->select(selectIdx, QItemSelectionModel::ClearAndSelect);
            this->csvEditor->scrollTo(selectIdx, QTableWidget::ScrollHint::PositionAtCenter);
        }

    });
}

void MainCSVTable::clear()
{
    this->csvEditor->blockSignals(true);
    
    // 現在のモデルをクリア
    if(currentModel) {
        currentModel->clearAll();
    }
    
    this->csvEditor->blockSignals(false);

    this->mainFileName->setText("");
    this->mainFileWordCount->setText("");
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
    this->currentModel = mainModel;
    if(mainModel == nullptr) {
        return;
    }


    this->csvEditor->horizontalHeader()->blockSignals(true);

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

    this->currentFileName = editedData;
    const auto& errors = this->runtimeData->errors;
    if(errors.empty() || errors.find(this->currentFileName) == errors.end()) {
        this->validateResultListButton->setEnabled(false);
    }
    else {
        this->validateResultListButton->setEnabled(true);
    }
    
    // プロキシモデルを初期化し、テーブルビューに設定
    if(_proxyModel == nullptr) {
        _proxyModel = new CSVEditorSortFilterProxyModel(this);
        connect(_proxyModel, &CSVEditorSortFilterProxyModel::columnVisibilityChanged,
                this, &MainCSVTable::restoreColumnWidths);
    }
    _proxyModel->setSortState(0, CSVEditorSortFilterProxyModel::SortOrder::None);
    _proxyModel->setSourceModel(mainModel);
    this->csvEditor->setModel(_proxyModel);
    mainModel->setSettings(this->setting);
    mainModel->setRuntimeData(this->runtimeData);
    
    // 言語列を追加
    size_t index = 1;
    for(auto langs = this->setting->languages; auto& lang : langs) {
        if(lang.enable == false) { continue; }
        mainModel->insertColumn(index, lang.languageName);
        index++;
    }
    _proxyModel->loadColumnFilterFromSettings();


    // ファイル名とワードカウントを更新
    this->mainFileName->setText(treeItemName);
    this->validateButton->setEnabled(true);
    
    QTimer::singleShot(0, this, [this]() {
        this->csvEditor->setUpdatesEnabled(false);
        this->csvEditor->resizeRowsToContents();
        this->restoreColumnWidths();
        this->csvEditor->setUpdatesEnabled(true);
        this->csvEditor->update();
    });


    auto selectionModel = this->csvEditor->selectionModel();
    connect(selectionModel, &QItemSelectionModel::selectionChanged, this, [this](const QItemSelection& selected, const QItemSelection& deselected)
    {
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
        auto editingDir = this->setting->langscoreProjectDirectory + "/editing";
    }
    else if(type == DispatchType::NotifyFinishValidateCSV)
    {
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

        QTimer::singleShot(1, [this]() {
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
