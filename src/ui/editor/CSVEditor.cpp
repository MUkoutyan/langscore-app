#include "CSVEditor.h"
#include "CSVEditorTableModel.h"
#include "MultiLineEditDelegate.h"
#include "../dialog/TranslationApiSettingsDialog.h"
#include "FastCSVContainer.h"
#include "../dialog/LanguageColumnSelectionDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFile>
#include <QKeyEvent>
#include <QClipboard>
#include <QGuiApplication>
#include <QKeySequence>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QUndoStack>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QMimeData>
#include <QSettings>
#include <QApplication>
#include <QInputDialog>
#include <QScrollArea>
#include <QDialog>
#include <QCheckBox>
#include <QPushButton>
#include <QScrollBar>
#include <QLabel>

using namespace langscore;


// CSVEditor implementation
CSVEditor::CSVEditor(std::weak_ptr<EditDataManager> loadFileManager, ComponentBase* component, QWidget* parent)
    : QTableView(parent)
    , ComponentBase(component)
    , loadFileManager(loadFileManager)
    , contextMenu(nullptr)
    , translationMenu(nullptr)
    , columnVisibilityMenu(nullptr)
    , translationManager(std::make_unique<TranslationManager>(this))
    , progressDialog(nullptr)
{
    this->setObjectName("CSVEditor");
    this->setSelectionBehavior(QAbstractItemView::SelectItems);
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->setSortingEnabled(false);
    this->setContextMenuPolicy(Qt::CustomContextMenu);

    this->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->verticalScrollBar()->setSingleStep(20);
    this->horizontalScrollBar()->setSingleStep(20);

    // マルチライン編集デリゲートを設定
    auto* multiLineDelegate = new MultiLineEditDelegate(this);
    this->setItemDelegate(multiLineDelegate);
    
    setupActions();
    setupContextMenu();
    
    connect(this, &QWidget::customContextMenuRequested, this, &CSVEditor::onCustomContextMenuRequested);
            
    connect(translationManager.get(), &TranslationManager::batchTranslationCompleted,
            this, &CSVEditor::onBatchTranslationCompleted);
    connect(translationManager.get(), &TranslationManager::batchTranslationError,
            this, &CSVEditor::onBatchTranslationError);
    connect(translationManager.get(), &TranslationManager::translationProgress,
            this, &CSVEditor::onTranslationProgress);

    auto hHeader = this->horizontalHeader();
    connect(hHeader, &QHeaderView::sectionResized, this, [this](int, int, int) {
        this->viewport()->update();
    });
    connect(hHeader, &QHeaderView::sectionClicked, this, &CSVEditor::onHeaderSectionClicked);
}

void CSVEditor::setModel(QAbstractItemModel* model)
{
    QTableView::setModel(model);
    connectModelSignals();
}

void CSVEditor::setupActions()
{
    cutAction = new QAction(tr("Cut"), this);
    cutAction->setShortcut(QKeySequence::Cut);
    connect(cutAction, &QAction::triggered, this, &CSVEditor::cut);
    
    copyAction = new QAction(tr("Copy"), this);
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, this, &CSVEditor::copy);
    
    pasteAction = new QAction(tr("Paste"), this);
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, this, &CSVEditor::paste);
    
    deleteAction = new QAction(tr("Delete"), this);
    deleteAction->setShortcut(Qt::Key_Delete);
    connect(deleteAction, &QAction::triggered, this, &CSVEditor::clearSelectedCells);
    
    selectAllAction = new QAction(tr("Select All"), this);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, this, &CSVEditor::selectAll);
    
    // Translation actions
    translationSettingsAction = new QAction(tr("Translation Settings..."), this);
    connect(translationSettingsAction, &QAction::triggered, this, &CSVEditor::openTranslationSettings);
    
    translateAction = new QAction(tr("Translate Selected Cells"), this);
    connect(translateAction, &QAction::triggered, this, &CSVEditor::translateSelectedCells);
    
    // Column visibility actions
    hideLanguageColumnsAction = new QAction(tr("Hide Language Columns..."), this);
    connect(hideLanguageColumnsAction, &QAction::triggered, this, &CSVEditor::hideLanguageColumns);
    
    filterSettingsAction = new QAction(tr("Filter Settings..."), this);
    connect(filterSettingsAction, &QAction::triggered, this, &CSVEditor::hideLanguageColumns);

    showAllColumnsAction = new QAction(tr("Show All Columns"), this);
    connect(showAllColumnsAction, &QAction::triggered, this, &CSVEditor::showAllColumns);
    
    // アクションを追加
    addAction(cutAction);
    addAction(copyAction);
    addAction(pasteAction);
    addAction(deleteAction);
    addAction(selectAllAction);
    addAction(translationSettingsAction);
    addAction(translateAction);
    addAction(hideLanguageColumnsAction);
    addAction(filterSettingsAction);
    addAction(showAllColumnsAction);
}

void CSVEditor::setupContextMenu()
{
    contextMenu = new QMenu(this);

    cutAction->setEnabled(true);
    copyAction->setEnabled(true);
    pasteAction->setEnabled(true);
    deleteAction->setEnabled(true);

    contextMenu->addAction(cutAction);
    contextMenu->addAction(copyAction);
    contextMenu->addAction(pasteAction);
    contextMenu->addAction(deleteAction);
    contextMenu->addSeparator();
    contextMenu->addAction(selectAllAction);
    
    // Translation menu
    contextMenu->addSeparator();
    translationMenu = contextMenu->addMenu(tr("Translation"));
    translationMenu->addAction(translateAction);
    translationMenu->addSeparator();
    translationMenu->addAction(translationSettingsAction);
    
    // Column visibility menu
    contextMenu->addSeparator();
    columnVisibilityMenu = contextMenu->addMenu(tr("Column Visibility"));
    columnVisibilityMenu->addAction(hideLanguageColumnsAction);
    columnVisibilityMenu->addAction(filterSettingsAction);
    columnVisibilityMenu->addAction(showAllColumnsAction);
    
    // Update menus
    updateTranslationMenu();
    updateColumnVisibilityMenu();
}

void CSVEditor::updateTranslationMenu()
{
    if (!translationMenu) return;
    
    bool hasService = hasConfiguredTranslationService();
    translationMenu->menuAction()->setVisible(hasService);
    
    if (hasService) {
        // Update translate action based on selection
        QList<QModelIndex> emptyCells = getEmptySelectedCells();
        translateAction->setEnabled(!emptyCells.isEmpty());
        
        QString actionText = tr("Translate Selected Cells");
        if (!emptyCells.isEmpty()) {
            actionText = tr("Translate %1 Empty Cell(s)").arg(emptyCells.size());
        }
        translateAction->setText(actionText);
    }
}

void CSVEditor::updateColumnVisibilityMenu()
{
    if (!columnVisibilityMenu || !model()) return;
    
    QStringList recognizedLanguages = getRecognizedLanguageCodesInColumns();
    
    // Enable/disable actions based on available columns
    hideLanguageColumnsAction->setEnabled(recognizedLanguages.isEmpty() == false);
    
    // Check if any columns are hidden
    bool hasHiddenColumns = false;
    const auto columns = model()->columnCount();
    for (int i = 0; i < columns; ++i) {
        if (isColumnHidden(i)) {
            hasHiddenColumns = true;
            break;
        }
    }
    showAllColumnsAction->setEnabled(hasHiddenColumns);
    
    // Update action text to show available languages
    if (!recognizedLanguages.isEmpty()) {
        QString actionText = tr("Hide Language Columns (%1)").arg(recognizedLanguages.join(", "));
        hideLanguageColumnsAction->setText(actionText);
    } else {
        hideLanguageColumnsAction->setText(tr("Hide Language Columns..."));
    }
}

void CSVEditor::hideLanguageColumns()
{
    if(model() == nullptr) { return; }
    
    QStringList recognizedLanguages = getRecognizedLanguageCodesInColumns();
    
    if (recognizedLanguages.isEmpty()) {
        QMessageBox::information(this, tr("Column Visibility"), tr("No recognizable language columns found."));
        return;
    }
    
    // Show dialog with checkboxes for language selection
    LanguageColumnSelectionDialog dialog(recognizedLanguages, this, this);
    
    if (dialog.exec() == QDialog::Accepted) {
        auto visibleState = dialog.getLanguagesVisibleState();
        
        if (visibleState.empty() == false) {
            changeColumnsVisibleWithLanguageCodes(visibleState);
            updateColumnVisibilityMenu();
        }
    }
}

void CSVEditor::showAllColumns()
{
    auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model());
    if(!proxy) { return; }
    proxy->clearAllHiddenLanguages();

    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.remove("filters");
    settings.sync();

    updateColumnVisibilityMenu();
}

QStringList CSVEditor::getRecognizedLanguageCodesInColumns() const
{
    auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model());
    QAbstractItemModel* m = proxy ? proxy->sourceModel() : model();
    if(!m) { return QStringList(); }

    QStringList recognizedLanguages;
    const auto columns = m->columnCount();
    
    const auto langNames = this->setting->languages
        | std::views::transform([](const auto& l) { return l.languageName; })
        | std::ranges::to<QStringList>();

    for(int col = 0; col < columns; ++col) 
    {
        QString headerText = m->headerData(col, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
        if(langNames.contains(headerText)) {
            recognizedLanguages.append(std::move(headerText));
        }
    }

    recognizedLanguages.removeDuplicates();
    return recognizedLanguages;
}

void CSVEditor::changeColumnsVisibleWithLanguageCodes(const std::vector<std::pair<QString, bool>>& languageCodes)
{
    auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model());
    if(!proxy) { return; }

    for(const auto& [lang, visible] : languageCodes) {
        proxy->setLanguageColumnHidden(lang, !visible);
    }
    proxy->saveColumnFilterToSettings(languageCodes);
}

bool CSVEditor::isLanguageColumnHidden(const QString& language) const
{
    auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model());
    if(proxy) {
        return proxy->isLanguageColumnHidden(language);
    }
    return false;
}

bool CSVEditor::hasConfiguredTranslationService() const
{
    return translationManager && translationManager->hasConfiguredService();
}

void CSVEditor::openTranslationSettings()
{
    this->dispatch(EnableBlur, {});
    TranslationApiSettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        updateTranslationMenu();
    }
    this->dispatch(DisableBlur, {});
}

void CSVEditor::translateSelectedCells()
{
    QList<QModelIndex> emptyCells = getEmptySelectedCells();
    if (emptyCells.isEmpty()) {
        QMessageBox::information(this, tr("Translation"), 
                               tr("No empty cells selected for translation."));
        return;
    }
    
    int originalColumn = findOriginalColumn();
    if (originalColumn == -1) {
        QMessageBox::warning(this, tr("Translation Error"), 
                           tr("Could not find 'Original' column for source text.(no use api)"));
        return;
    }
    
    // Determine which translation service to use
    TranslationService::ServiceType preferredService = TranslationService::ServiceType::DeepL;
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    QString deeplKey = settings.value("translation/deeplApiKey", "").toString();
    QString googleKey = settings.value("translation/googleApiKey", "").toString();
    
    // Use Google Translate if DeepL key is not available but Google key is
    if (deeplKey.isEmpty() && !googleKey.isEmpty()) {
        preferredService = TranslationService::ServiceType::GoogleTranslate;
    }
    
    // Prepare translation requests
    QList<TranslationManager::BatchTranslationRequest> requests;

    QString sourceLang = TranslationService::GetTranslationLanguageCode(preferredService, this->setting->defaultLanguage);
    bool detectSameLang = false;
    for (const QModelIndex& cellIndex : emptyCells) {
        QString originalText = getOriginalTextForRow(cellIndex.row());
        QString targetLang = getTargetLanguageForColumn(cellIndex.column(), preferredService);
        
        if (originalText.isEmpty()) {
            continue; 
        }
        
        if (targetLang.isEmpty()) {
            continue;
        }

        if(sourceLang == targetLang) {
            detectSameLang = true;
        }
        
        TranslationManager::BatchTranslationRequest request;
        request.text = originalText;
        request.sourceLang = sourceLang;
        request.targetLang = targetLang;
        request.row = cellIndex.row();
        request.column = cellIndex.column();
        
        requests.append(std::move(request));
    }
    
    if (requests.isEmpty()) {
        QMessageBox::warning(this, tr("Translation Error"), 
                           tr("No valid translation requests could be prepared.(no use api)"));
        return;
    }

    if(detectSameLang) 
    {
        auto button = QMessageBox::information(
            //翻訳する言語と翻訳後の言語が同じセルがあります。続けますか？
            this, tr("Translation"), tr("There are cells where the source language and the target language are the same.\nDo you want to continue?"), 
            QMessageBox::Yes, QMessageBox::No
        );
        if(button == QMessageBox::No) {
            return;
        }
    }
    
    // Show progress dialog
    if (progressDialog == nullptr) {
        progressDialog = new TranslationProgressDialog(this);
        connect(progressDialog, &TranslationProgressDialog::cancelled, this, [this]() {
            // Handle cancellation if needed
        });
    }
    
    progressDialog->startProgress(requests.size());
    progressDialog->show();
    
    // Start translation with the preferred service
    translationManager->translateBatch(requests, preferredService);
}

QList<QModelIndex> CSVEditor::getEmptySelectedCells() const
{
    QList<QModelIndex> emptyCells;
    QModelIndexList selectedIndexes = getSelectedIndexes();
    
    for (const QModelIndex& index : selectedIndexes) {
        if (index.isValid()) {
            QString cellText = model()->data(index, Qt::EditRole).toString().trimmed();
            if (cellText.isEmpty()) {
                emptyCells.append(index);
            }
        }
    }
    
    return emptyCells;
}

QString CSVEditor::getOriginalTextForRow(int row) const
{
    int originalColumn = findOriginalColumn();
    if (originalColumn == -1 || !model()) return QString();
    
    QModelIndex originalIndex = model()->index(row, originalColumn);
    if (!originalIndex.isValid()) return QString();
    
    return model()->data(originalIndex, Qt::EditRole).toString().trimmed();
}

QString CSVEditor::getTargetLanguageForColumn(int column, TranslationService::ServiceType serviceType) const
{
    if (!model()) return QString();
    
    QString headerText = model()->headerData(column, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
    return TranslationService::GetTranslationLanguageCode(serviceType, headerText);
}

int CSVEditor::findOriginalColumn() const
{
    auto columns = this->model()->columnCount();
    for(int i = 0; i < columns; ++i) {
        auto header_id = this->model()->headerData(i, Qt::Horizontal, Qt::UserRole).toString();
        if(header_id == Column_ID_Original) {
            return i;
        }
    }
    return -1;
}

void CSVEditor::onBatchTranslationCompleted(int batchId, const QList<TranslationManager::BatchTranslationResult>& results)
{
    if (!model()) return;
    
    QList<CSVEditCommand::CellEdit> edits;
    int successCount = 0;
    
    for (const auto& result : results) {
        if (result.success && !result.translatedText.isEmpty()) {
            QModelIndex cellIndex = model()->index(result.row, result.column);
            if (cellIndex.isValid()) {
                CSVEditCommand::CellEdit edit;
                edit.index = cellIndex;
                edit.oldValue = model()->data(cellIndex, Qt::EditRole);
                edit.newValue = result.translatedText;
                edits.append(edit);
                successCount++;
            }
        }
    }
    
    if (!edits.isEmpty()) {
        executeEditCommand(edits, tr("Translate Cells"));
    }
    
    if (progressDialog) {
        progressDialog->showCompletion(successCount, results.size());
    }
    
    updateTranslationMenu();
}

void CSVEditor::onBatchTranslationError(int batchId, const QString& errorMessage)
{
    if (progressDialog) {
        progressDialog->showError(errorMessage);
    } else {
        QMessageBox::warning(this, tr("Translation Error"), errorMessage);
    }
}

void CSVEditor::onTranslationProgress(int batchId, int completed, int total)
{
    if (progressDialog) {
        progressDialog->updateProgress(completed, total);
    }
}

void CSVEditor::connectModelSignals()
{
    if(!model()) { return; }
    connect(model(), &QAbstractItemModel::dataChanged,
            this, &CSVEditor::onModelDataChanged, Qt::UniqueConnection);

    if(auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model())) {
        connect(proxy, &CSVEditorSortFilterProxyModel::sortStateChanged,
                this,  &CSVEditor::sortStateChanged, Qt::UniqueConnection);
    }
}

void CSVEditor::sortStateChanged(int col, SortOrder order)
{
    auto* h = horizontalHeader();
    if(order == SortOrder::None) {
        h->setSortIndicatorShown(false);
    }
    else {
        h->setSortIndicatorShown(true);
        h->setSortIndicator(col, order == SortOrder::Ascending
            ? Qt::AscendingOrder : Qt::DescendingOrder);
    }
}

void CSVEditor::onModelDataChanged()
{
    if (_suppressUndoTracking == false) {
        _isModified = true;
    }
    updateTranslationMenu(); // Update menu when data changes
    updateColumnVisibilityMenu(); // Update column visibility menu when data changes
}

void CSVEditor::selectAndEditNextCell()
{
    auto currentSelectedIndexes = getSelectedIndexes();
    if(currentSelectedIndexes.isEmpty()) { return; }

    auto index = currentSelectedIndexes.back();
    if(this->model()->rowCount() <= index.row()) {
        return;
    }

    this->selectionModel()->setCurrentIndex(index, QItemSelectionModel::SelectionFlag::Deselect);

    auto newSelectIndex = this->model()->index(index.row() + 1, index.column(), index.parent());
    this->selectionModel()->setCurrentIndex(newSelectIndex, QItemSelectionModel::SelectionFlag::Select);
    this->edit(newSelectIndex);
}

void CSVEditor::keyPressEvent(QKeyEvent* event)
{
    auto currentSelectedIndexes = getSelectedIndexes();
    bool canEdit = true;
    for(auto& index : currentSelectedIndexes)
    {
        if(index.isValid() == false) { continue; }

        auto flag = model()->flags(index);
        canEdit &= (flag & Qt::ItemIsEditable) != 0;
    }

    if(canEdit == false)
    {
        if(event->matches(QKeySequence::Cut) ||
           event->matches(QKeySequence::Paste) ||
           event->key() == Qt::Key_Delete)
        {
            QTableView::keyPressEvent(event);
            return;
        }
    }

    if (event->matches(QKeySequence::Copy)) {
        copy();
        return;
    } else if (event->matches(QKeySequence::Cut)) {
        cut();
        return;
    } else if (event->matches(QKeySequence::Paste)) {
        paste();
        return;
    } else if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        return;
    } else if (event->matches(QKeySequence::Undo)) {
        undo();
        return;
    } else if (event->matches(QKeySequence::Redo)) {
        redo();
        return;
    } else if (event->key() == Qt::Key_Delete) {
        clearSelectedCells();
        return;
    }
    else if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if(currentSelectedIndexes.isEmpty() == false) {
            edit(currentSelectedIndexes[0]);
            return;
        }
    }
    
    QTableView::keyPressEvent(event);
}

void CSVEditor::contextMenuEvent(QContextMenuEvent* event)
{
    this->showContextMenu(event->globalPos());
}

void CSVEditor::onCustomContextMenuRequested(const QPoint& pos)
{
    this->showContextMenu(mapToGlobal(pos));
}

void CSVEditor::showContextMenu(const QPoint& globalPos)
{
    if(contextMenu) 
    {
        auto currentSelectedIndexes = getSelectedIndexes();
        for(auto& index : currentSelectedIndexes)
        {
            if(index.isValid() == false) { continue; }

            auto flag = model()->flags(index);
            bool isEditable = (flag & Qt::ItemIsEditable) != 0;
            cutAction->setEnabled(isEditable);
            pasteAction->setEnabled(isEditable);
            deleteAction->setEnabled(isEditable);
        } 
        
        // Update translation menu
        updateTranslationMenu();
        
        // Update column visibility menu
        updateColumnVisibilityMenu();

        contextMenu->exec(globalPos);
    }
}

bool CSVEditor::saveCSV(const QString& filePath, langscore::CSVEditorTableModel* csvModel)
{
    if (!csvModel) {
        return false;
    }
    
    QString saveFilePath = filePath.isEmpty() ? currentFilePath : filePath;
    if (saveFilePath.isEmpty()) {
        return saveAsCSV(QString(), csvModel);
    }
    
    bool success = csvModel->saveToFile(saveFilePath);
    if (success) {
        currentFilePath = saveFilePath;
        _isModified = false;
    }
    
    return success;
}

bool CSVEditor::saveAsCSV(const QString& filePath, langscore::CSVEditorTableModel* csvModel)
{
    QString saveFilePath = filePath;
    if (saveFilePath.isEmpty()) {
        saveFilePath = QFileDialog::getSaveFileName(this, tr("Save CSV File"), 
                                                   currentFilePath, tr("CSV Files (*.csv)"));
        if (saveFilePath.isEmpty()) {
            return false;
        }
    }
    
    return saveCSV(saveFilePath, csvModel);
}

void CSVEditor::setText(QModelIndex index, QString newText)
{
    CSVEditCommand::CellEdit edit;
    edit.index = index;
    edit.oldValue = model()->data(index, Qt::EditRole);
    edit.newValue = newText;
    if(edit.oldValue == edit.newValue) { return; }

    executeEditCommand({edit}, QString{});
    QTimer::singleShot(1, [this, row = index.row()]() {
        this->resizeRowToContents(row);
    });
}

// Edit operations
void CSVEditor::clearSelectedCells()
{
    QModelIndexList indexes = getSelectedIndexes();
    if (indexes.isEmpty()) return;

    QList<CSVEditCommand::CellEdit> edits;
    for (const auto& index : indexes) {
        if (index.isValid()) {
            CSVEditCommand::CellEdit edit;
            edit.index = index;
            edit.oldValue = model()->data(index, Qt::EditRole);
            edit.newValue = QString();
            edits.append(edit);
        }
    }

    if (edits.isEmpty() == false) {
        executeEditCommand(edits, tr("Clear Cells"));
    }
}

// Clipboard operations
void CSVEditor::cut()
{
    copy();
    clearSelectedCells();
}

void CSVEditor::copy()
{
    QString text = getSelectedCellsAsText();
    if (!text.isEmpty()) {
        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(text);
    }
}

void CSVEditor::paste()
{
    QClipboard* clipboard = QGuiApplication::clipboard();
    QString text = clipboard->text();
    if (!text.isEmpty()) {
        QModelIndexList indexes = getSelectedIndexes();
        if (indexes.isEmpty()) return;

        // 選択されたセルを行・列順にソート
        std::sort(indexes.begin(), indexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
            if(a.row() != b.row()) {
                return a.row() < b.row();
            }
            return a.column() < b.column();
        });

        // 選択範囲の情報を取得
        int minRow = indexes.first().row();
        int minCol = indexes.first().column();

        // CSV形式のテキストを正しく行ごとに分割
        QStringList lines = parseCSVRows(text);
        
        QList<CSVEditCommand::CellEdit> edits;

        for(int lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
            int targetRow = minRow + lineIndex;
            if(targetRow >= model()->rowCount()) {
                break;
            }

            // CSV行を解析
            QStringList fields = parseCSVLine(lines[lineIndex]);

            for(int fieldIndex = 0; fieldIndex < fields.size(); ++fieldIndex) {
                int targetColumn = minCol + fieldIndex;
                if(targetColumn >= model()->columnCount()) {
                    break;
                }

                QModelIndex targetIndex = model()->index(targetRow, targetColumn);
                if (targetIndex.isValid()) {
                    CSVEditCommand::CellEdit edit;
                    edit.index = targetIndex;
                    edit.oldValue = model()->data(targetIndex, Qt::EditRole);
                    edit.newValue = fields[fieldIndex];
                    edits.append(edit);
                }
            }
        }

        if (!edits.isEmpty()) {
            executeEditCommand(edits, tr("Paste"));
        }
    }
}

// Undo/Redo
void CSVEditor::undo()
{
    if (history) {
        history->undo();
    }
}

void CSVEditor::redo()
{
    if (history) {
        history->redo();
    }
}

bool CSVEditor::canUndo() const
{
    return history ? history->canUndo() : false;
}

bool CSVEditor::canRedo() const
{
    return history ? history->canRedo() : false;
}

// Selection operations
void CSVEditor::selectAll()
{
    QTableView::selectAll();
}

// Private helper methods
void CSVEditor::executeEditCommand(const QList<CSVEditCommand::CellEdit>& edits, const QString& description)
{
    if (edits.isEmpty() || !history) return;
    
    auto* command = new CSVEditCommand(this, edits, description);
    history->push(command);
}

QModelIndexList CSVEditor::getSelectedIndexes() const
{
    if(selectionModel() == nullptr) { return QModelIndexList{}; }
    return selectionModel()->selectedIndexes();
}

QString CSVEditor::formatCSVField(const QString& field) const
{
    // CSVフィールドをエスケープ
    if(field.contains(',') || field.contains('"') || field.contains('\n') || field.contains('\r')) {
        QString escaped = field;
        escaped.replace("\"", "\"\"");  // ダブルクォートをエスケープ
        return "\"" + escaped + "\"";
    }
    return field;
}

QStringList CSVEditor::parseCSVLine(const QString& line) const
{
    QStringList result;
    QString currentField;
    bool inQuotes = false;

    for(int i = 0; i < line.length(); ++i) {
        QChar ch = line.at(i);

        if(ch == '"') {
            if(inQuotes) {
                // ダブルクォート内で次の文字もダブルクォートの場合はエスケープ
                if(i + 1 < line.length() && line.at(i + 1) == '"') {
                    currentField += '"';
                    i++; // 次のダブルクォートをスキップ
                }
                else {
                    inQuotes = false;
                }
            }
            else {
                inQuotes = true;
            }
        }
        else if(ch == ',' && !inQuotes) {
            result.append(currentField);
            currentField.clear();
        }
        else {
            currentField += ch;
        }
    }

    result.append(currentField);
    return result;
}

QStringList CSVEditor::parseCSVRows(const QString& text) const
{
    QStringList rows;
    QString currentRow;
    bool inQuotes = false;

    for(int i = 0; i < text.length(); ++i) {
        QChar ch = text.at(i);

        if(ch == '"') {
            currentRow += ch;
            if(inQuotes) {
                // ダブルクォート内で次の文字もダブルクォートの場合はエスケープ
                if(i + 1 < text.length() && text.at(i + 1) == '"') {
                    currentRow += text.at(i + 1);
                    i++; // 次のダブルクォートをスキップ
                }
                else {
                    inQuotes = false;
                }
            }
            else {
                inQuotes = true;
            }
        }
        else if(ch == '\n' && !inQuotes) {
            // クォート外の改行は行の区切り
            rows.append(currentRow);
            currentRow.clear();
        }
        else {
            currentRow += ch;
        }
    }

    // 最後の行を追加（改行で終わっていない場合）
    if(!currentRow.isEmpty()) {
        rows.append(currentRow);
    }

    return rows;
}

QString CSVEditor::getSelectedCellsAsText() const
{
    QModelIndexList indexes = getSelectedIndexes();
    if(indexes.isEmpty()) {
        return QString();
    }

    // インデックスを行・列順にソート
    std::sort(indexes.begin(), indexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        if(a.row() != b.row()) {
            return a.row() < b.row();
        }
        return a.column() < b.column();
        });

    // 選択された範囲を特定
    int minRow = indexes.first().row();
    int maxRow = indexes.last().row();
    int minCol = indexes.first().column();
    int maxCol = indexes.last().column();

    // 選択されたセルを行・列のマップに整理
    QMap<int, QMap<int, QString>> cellData;
    for(const auto& index : indexes) {
        QString cellText = model()->data(index, Qt::DisplayRole).toString();
        cellData[index.row()][index.column()] = cellText;
    }

    QString result;
    for(int row = minRow; row <= maxRow; ++row) {
        if(!cellData.contains(row)) {
            continue; // この行に選択されたセルがない場合はスキップ
        }

        QStringList rowFields;
        for(int col = minCol; col <= maxCol; ++col) {
            QString cellText = cellData[row].value(col, QString());
            rowFields.append(formatCSVField(cellText));
        }

        result += rowFields.join(",");
        if(row < maxRow) {
            result += "\n";
        }
    }

    return result;
}

void CSVEditor::setData(QModelIndex index, QVariant value, int role)
{
    this->model()->setData(index, value, role);
}

void CSVEditor::loadColumnFilterSettings()
{
    auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model());
    if(proxy) {
        proxy->loadColumnFilterFromSettings();
    }
}

void CSVEditor::onHeaderSectionClicked(int logicalIndex)
{
    if(auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model())) {
        proxy->cycleColumnSort(logicalIndex);
    }
}