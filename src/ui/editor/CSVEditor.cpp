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
#include <QPainter>
#include <QAbstractProxyModel>
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
#include <QAbstractProxyModel>

using namespace langscore;

namespace
{
    //エディター内でコピーした表。貼り付けるときは表の形をそのまま使う
    const QString CellsMimeType = "application/x-langscore-cells";

    //区切り文字・"・改行を含む値は " で囲み、" は "" にする
    QString FormatCell(const QString& field, QChar separator)
    {
        if(field.contains(separator) || field.contains('"') || field.contains('\n') || field.contains('\r')) {
            return QString('"') + QString(field).replace("\"", "\"\"") + '"';
        }
        return field;
    }

    //区切り文字と改行で区切った表を読む。
    //" で始まる値は次の " までを1つの値とし（区切り文字・改行を含められる）、"" は " にする。値の途中の " はそのまま残す
    QList<QStringList> ParseCells(const QString& text, QChar separator)
    {
        QList<QStringList> rows;
        QStringList row;
        QString field;
        bool inQuotes = false;
        bool fieldStart = true;
        for(qsizetype i = 0; i < text.size(); ++i)
        {
            const QChar ch = text[i];
            if(inQuotes)
            {
                if(ch != '"') { field += ch; }
                else if(i + 1 < text.size() && text[i + 1] == '"') { field += '"'; ++i; }
                else { inQuotes = false; }
                continue;
            }
            if(ch == '"' && fieldStart) {
                inQuotes = true;
                fieldStart = false;
            }
            else if(ch == separator || ch == '\n')
            {
                row.append(field);
                field.clear();
                fieldStart = true;
                if(ch == '\n') {
                    rows.append(row);
                    row.clear();
                }
            }
            else {
                field += ch;
                fieldStart = false;
            }
        }
        //改行で終わっていない最後の行
        if(fieldStart == false || row.isEmpty() == false) {
            row.append(field);
            rows.append(row);
        }
        return rows;
    }
}


// CSVEditor implementation
CSVEditor::CSVEditor(ComponentBase* component, QWidget* parent)
    : QTableView(parent)
    , ComponentBase(component)
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
    //セルを選んで文字を打つだけで入力を始められるようにする（IME の入力も含む）
    this->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::AnyKeyPressed);

    //行の番号を出す。行の高さは表示したときに内容に合わせ、その後は手で変えられる
    this->verticalHeader()->setVisible(true);
    this->verticalHeader()->setSectionResizeMode(QHeaderView::Interactive);

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

    //ファイルツリーなど、表の外にフォーカスがあるときは反応しないようにする
    for(auto* action : {cutAction, copyAction, pasteAction, deleteAction, selectAllAction}) {
        action->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    }

    // Translation actions
    translationSettingsAction = new QAction(tr("Translation Settings..."), this);
    connect(translationSettingsAction, &QAction::triggered, this, &CSVEditor::openTranslationSettings);
    
    translateAction = new QAction(tr("Translate Selected Cells"), this);
    connect(translateAction, &QAction::triggered, this, &CSVEditor::translateSelectedCells);

    retranslateAction = new QAction(tr("Retranslate Selected Cells (Overwrite)"), this);
    connect(retranslateAction, &QAction::triggered, this, &CSVEditor::retranslateSelectedCells);

    //ショートカットはメニューバーの「編集」に付いているため、ここでは付けない（二重に付けると両方効かなくなる）
    undoAction = history->createUndoAction(this, tr("Undo"));
    redoAction = history->createRedoAction(this, tr("Redo"));

    // Column visibility actions
    hideLanguageColumnsAction = new QAction(tr("Hide Language Columns..."), this);
    connect(hideLanguageColumnsAction, &QAction::triggered, this, &CSVEditor::hideLanguageColumns);
    
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
    addAction(retranslateAction);
    addAction(hideLanguageColumnsAction);
addAction(showAllColumnsAction);
}

void CSVEditor::setupContextMenu()
{
    contextMenu = new QMenu(this);

    contextMenu->addAction(undoAction);
    contextMenu->addAction(redoAction);
    contextMenu->addSeparator();

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
    translationMenu->addAction(retranslateAction);
    translationMenu->addSeparator();
    translationMenu->addAction(translationSettingsAction);
    
    // Column visibility menu
    contextMenu->addSeparator();
    columnVisibilityMenu = contextMenu->addMenu(tr("Show Columns"));
    columnVisibilityMenu->addAction(hideLanguageColumnsAction);
columnVisibilityMenu->addAction(showAllColumnsAction);
    
    // Update menus
    updateTranslationMenu();
    updateColumnVisibilityMenu();
}

void CSVEditor::updateTranslationMenu()
{
    if (!translationMenu) return;
    
    //外部翻訳サービスが未設定でも、設定を開けるようにメニューは常に出す
    const bool hasService = hasConfiguredTranslationService();

    const auto emptyCells = getEmptySelectedCells();
    translateAction->setEnabled(hasService && emptyCells.isEmpty() == false);
    QString actionText = tr("Translate Selected Cells");
    if(hasService == false) {
        actionText = tr("Translate Selected Cells (Set up translation service first)");
    }
    else if(emptyCells.isEmpty() == false) {
        actionText = tr("Translate %1 Empty Cell(s)").arg(emptyCells.size());
    }
    translateAction->setText(actionText);

    retranslateAction->setEnabled(hasService && getTranslatableSelectedCells(true).isEmpty() == false);
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
        QMessageBox::information(this, tr("Show Columns"), tr("No recognizable language columns found."));
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
    settings.remove(proxy->settingsGroup());
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
    //翻訳中に次の翻訳を始めると、表示上の行・列をキーにした管理が衝突するため受け付けない
    if(pendingTranslationCells.isEmpty() == false) {
        QMessageBox::information(this, tr("Translation"), tr("Translation is in progress."));
        return;
    }

    QList<QModelIndex> emptyCells = getEmptySelectedCells();
    if (emptyCells.isEmpty()) {
        QMessageBox::information(this, tr("Translation"),
                               tr("No empty cells selected for translation."));
        return;
    }
    translateCells(emptyCells);
}

//選択したセルを、翻訳済みのものも含めて翻訳し直す
void CSVEditor::retranslateSelectedCells()
{
    if(pendingTranslationCells.isEmpty() == false) {
        QMessageBox::information(this, tr("Translation"), tr("Translation is in progress."));
        return;
    }
    const auto targetCells = getTranslatableSelectedCells(true);
    if(targetCells.isEmpty()) { return; }

    const auto button = QMessageBox::question(
        this, tr("Translation"),
        tr("The existing translations of %1 cell(s) will be overwritten.\nDo you want to continue?").arg(targetCells.size())
    );
    if(button != QMessageBox::Yes) { return; }
    translateCells(targetCells);
}

void CSVEditor::translateCells(const QList<QModelIndex>& targetCells)
{
    int originalColumn= findOriginalColumn();
    if (originalColumn == -1) {
        QMessageBox::warning(this, tr("Translation Error"), 
                           tr("Could not find the 'original' column for the source text. (The API was not used.)"));
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
    for (const QModelIndex& cellIndex : targetCells) {
        QString originalText= getOriginalTextForRow(cellIndex.row());
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
        pendingTranslationCells[{request.row, request.column}] = QPersistentModelIndex(cellIndex);

        requests.append(std::move(request));
    }
    
    if (requests.isEmpty()) {
        QMessageBox::warning(this, tr("Translation Error"), 
                           tr("No valid translation requests could be prepared. (The API was not used.)"));
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
            //登録したセルを残すと、翻訳中と判定されて次の翻訳ができなくなる
            pendingTranslationCells.clear();
            return;
        }
    }
    
    // Show progress dialog
    if (progressDialog == nullptr) {
        progressDialog = new TranslationProgressDialog(this);
        connect(progressDialog, &TranslationProgressDialog::cancelled, this, [this]() {
            //残りのリクエストを送信せず、届いた結果もセルに書き込まない
            translationManager->cancelAll();
            pendingTranslationCells.clear();
        });
    }
    
    progressDialog->startProgress(requests.size());
    progressDialog->show();
    
    // Start translation with the preferred service
    translationManager->translateBatch(requests, preferredService);
}

//翻訳の対象にできる、選択中の言語列のセル。includeTranslatedがfalseなら空のセルのみ対象とする。
QList<QModelIndex> CSVEditor::getTranslatableSelectedCells(bool includeTranslated) const
{
    QList<QModelIndex> result;
    if(model() == nullptr) { return result; }
    for(const QModelIndex& index : getSelectedIndexes())
    {
        if(index.isValid() == false) { continue; }
        if((model()->flags(index) & Qt::ItemIsEditable) == 0) { continue; }
        const bool isEmpty = model()->data(index, Qt::EditRole).toString().trimmed().isEmpty();
        if(includeTranslated || isEmpty) {
            result.append(index);
        }
    }
    return result;
}

QList<QModelIndex> CSVEditor::getEmptySelectedCells() const
{
    return getTranslatableSelectedCells(false);
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
    //どのセルで何が起きたかを、進捗ダイアログに1行ずつ出す
    const auto addMessage = [this](const QString& kind, const TranslationManager::BatchTranslationResult& result, const QString& message) {
        if(progressDialog == nullptr) { return; }
        const auto language = model()->headerData(result.column, Qt::Horizontal, Qt::DisplayRole).toString();
        //[エラー] 3行目 (en): メッセージ
        progressDialog->addMessage(tr("[%1] Row %2 (%3): %4").arg(kind).arg(result.row + 1).arg(language, message));
    };

    for (const auto& result : results) {
        //表示が変更されても同一のセルを参照できるように、QPersistentModelIndexを使用すること。
        const QPersistentModelIndex cellIndex = pendingTranslationCells.take({result.row, result.column});
        if (result.success == false) {
            addMessage(tr("Error"), result, result.errorMessage.isEmpty() ? tr("Unknown error") : result.errorMessage);
        }
        else if (result.translatedText.isEmpty()) {
            //翻訳結果が空でした
            addMessage(tr("Warning"), result, tr("The translation result was empty."));
        }
        else if (cellIndex.isValid() == false || cellIndex.model() != model()) {
            //翻訳中に表が切り替わったため、セルに反映できませんでした
            addMessage(tr("Warning"), result, tr("The table was switched during translation, so the result could not be applied."));
        }
        else {
            CSVEditCommand::CellEdit edit;
            edit.index = cellIndex;
            edit.oldValue = model()->data(cellIndex, Qt::EditRole);
            edit.newValue = result.translatedText;
            edits.append(edit);
            successCount++;
        }
    }
    
    if (!edits.isEmpty()) {
        executeEditCommand(edits, tr("Translate Cells"));
    }
    
    if (progressDialog) {
        //個々の内容はメッセージ欄に出しているため、状態の欄には件数だけを出す
        progressDialog->showCompletion(successCount, results.size());
    }
    
    updateTranslationMenu();
}

void CSVEditor::onBatchTranslationError(int batchId, const QString& errorMessage)
{
    //翻訳を始められなかった場合。登録したセルを残すと次の翻訳ができなくなる
    pendingTranslationCells.clear();
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

void CSVEditor::selectAndEditNextCell(const QModelIndex& from, int rowStep, int columnStep)
{
    if(model() == nullptr) { return; }
    if(from.isValid() == false || from.model() != model()) { return; }

    //編集できない列（原文・種類）は飛ばす。表の端まで来たら何もしない
    int row = from.row() + rowStep;
    int column = from.column() + columnStep;
    while(0 <= row && row < model()->rowCount() && 0 <= column && column < model()->columnCount())
    {
        const auto nextIndex = model()->index(row, column);
        if(isEditableIndex(nextIndex))
        {
            this->selectionModel()->setCurrentIndex(nextIndex, QItemSelectionModel::ClearAndSelect);
            this->scrollTo(nextIndex);
            this->edit(nextIndex);
            return;
        }
        row += rowStep;
        column += columnStep;
    }
}

void CSVEditor::cellEditsApplied(const QModelIndex& firstIndex)
{
    //Undo/Redo したセルが画面の外にあっても分かるように、見える位置まで移動する
    auto viewIndex = firstIndex;
    if(auto* proxy = qobject_cast<QAbstractProxyModel*>(model()); proxy && firstIndex.model() == proxy->sourceModel()) {
        viewIndex = proxy->mapFromSource(firstIndex);
    }
    if(viewIndex.isValid() && viewIndex.model() == model()) {
        this->scrollTo(viewIndex);
        return;
    }
    //表示していないファイル、または絞り込みで隠れているセル
    this->dispatch(StatusMessage, {tr("Cells that are not displayed were changed."), 5000});
}

bool CSVEditor::edit(const QModelIndex& index, EditTrigger trigger, QEvent* event)
{
    //文字を打って編集を始めたときは、表計算ソフトと同じく元の文章を置き換える（MultiLineEditDelegate::setEditorData）
    _replaceTextOnEdit = (trigger == AnyKeyPressed);
    const bool result = QTableView::edit(index, trigger, event);
    _replaceTextOnEdit = false;
    return result;
}

bool CSVEditor::isEditableIndex(const QModelIndex& index)
{
    if(index.isValid() == false) { return false; }
    return (model()->flags(index) & Qt::ItemIsEditable) != 0;
}

void CSVEditor::keyPressEvent(QKeyEvent* event)
{
    //編集できないセルは cut・paste・clearSelectedCells の中で飛ばすため、ここでは選択の内容を見ない
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
        //選択範囲の並びは選んだ順とは限らないため、今のセル（最後に選んだセル）を開く
        if(currentIndex().isValid()) {
            edit(currentIndex());
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
        const bool hasEditableCell = std::ranges::any_of(currentSelectedIndexes, [this](const QModelIndex& index) {
            return isEditableIndex(index);
        });
        cutAction->setEnabled(hasEditableCell);
        pasteAction->setEnabled(hasEditableCell);
        deleteAction->setEnabled(hasEditableCell);
        
        // Update translation menu
        updateTranslationMenu();
        
        // Update column visibility menu
        updateColumnVisibilityMenu();

        contextMenu->exec(globalPos);
    }
}

void CSVEditor::setText(QModelIndex index, QString newText)
{
    CSVEditCommand::CellEdit edit;
    edit.index = index;
    edit.oldValue = model()->data(index, Qt::EditRole);
    edit.newValue = newText;
    if(edit.oldValue == edit.newValue) { return; }

    executeEditCommand({edit}, QString{});
    QTimer::singleShot(1, this, [this, row = index.row()]() {
        this->resizeRowToContents(row);
    });
}

// Edit operations
void CSVEditor::clearSelectedCells()
{
    QModelIndexList indexes = getSelectedIndexes();
    if(indexes.isEmpty()) { return; }

    QList<CSVEditCommand::CellEdit> edits;
    for (const auto& index : indexes) {
        if (this->isEditableIndex(index)) {
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
    const auto indexes = getSelectedIndexes();
    if(indexes.isEmpty()) { return; }

    auto* mimeData = new QMimeData();
    mimeData->setData(CellsMimeType, getSelectedCellsAsText(',').toUtf8());
    //他のアプリには、1つのセルなら値そのもの、複数ならタブ区切り（Excel などと同じ形式）で渡す
    mimeData->setText(indexes.size() == 1 ? model()->data(indexes.front(), Qt::DisplayRole).toString() : getSelectedCellsAsText('\t'));
    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void CSVEditor::paste()
{
    const auto* mimeData = QGuiApplication::clipboard()->mimeData();
    if(mimeData == nullptr) { return; }
    QModelIndexList indexes = getSelectedIndexes();
    if(indexes.isEmpty()) { return; }

    //Windowsのクリップボードは改行が\r\nなので、\nにそろえてから分割する
    const auto toLF = [](QString text) { return text.replace("\r\n", "\n"); };
    QList<QStringList> cells;
    if(mimeData->hasFormat(CellsMimeType))
    {
        cells = ParseCells(toLF(QString::fromUtf8(mimeData->data(CellsMimeType))), ',');
    }
    else
    {
        auto text = toLF(mimeData->text());
        if(text.isEmpty()) { return; }
        if(text.contains('\t')) {
            //Excel などの表計算ソフトからコピーした表
            cells = ParseCells(text, '\t');
        }
        else {
            //それ以外は、カンマや改行を含めて1つの値として扱う。コピーした際に付く末尾の改行は除く
            if(text.endsWith('\n')) { text.chop(1); }
            cells = {{text}};
        }
    }
    if(cells.isEmpty()) { return; }

    QList<CSVEditCommand::CellEdit> edits;
    const auto addEdit = [this, &edits](const QModelIndex& target, const QString& value) {
        if(isEditableIndex(target) == false) { return; }
        edits.append({target, model()->data(target, Qt::EditRole), value});
    };

    //表に入りきらなかった分は捨てて通知する。
    bool overflowed = false;
    if(cells.size() == 1 && cells.front().size() == 1)
    {
        //1つの値は、選択したすべてのセルに入れる
        for(const auto& targetIndex : indexes) {
            addEdit(targetIndex, cells.front().front());
        }
    }
    else
    {
        //表は、選択範囲の左上から貼り付ける
        const int minRow = std::ranges::min(indexes, {}, &QModelIndex::row).row();
        const int minColumn = std::ranges::min(indexes, {}, &QModelIndex::column).column();

        for(int rowOffset = 0; rowOffset < cells.size(); ++rowOffset)
        {
            const int targetRow = minRow + rowOffset;
            if(targetRow >= model()->rowCount()) {
                overflowed = true;
                break;
            }
            const auto& fields = cells[rowOffset];
            for(int columnOffset = 0; columnOffset < fields.size(); ++columnOffset)
            {
                const int targetColumn = minColumn + columnOffset;
                if(targetColumn >= model()->columnCount()) {
                    overflowed = true;
                    break;
                }
                addEdit(model()->index(targetRow, targetColumn), fields[columnOffset]);
            }
        }
    }

    if(edits.isEmpty() == false) {
        executeEditCommand(edits, tr("Paste"));
    }
    if(overflowed) {
        this->dispatch(StatusMessage, {tr("Some of the pasted cells were outside the table and were not pasted."), 5000});
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

// Selection operations
void CSVEditor::selectAll()
{
    QTableView::selectAll();
}

// Private helper methods
void CSVEditor::executeEditCommand(const QList<CSVEditCommand::CellEdit>& edits, const QString& description)
{
    if (edits.isEmpty() || !history) return;

    //表示用の proxy のセルは、ファイルの切り替えや列を隠したときに無効になるため、元のモデルのセルで記録する
    auto sourceEdits = edits;
    if(auto* proxy = qobject_cast<QAbstractProxyModel*>(model())) {
        for(auto& edit : sourceEdits) {
            if(edit.index.model() == proxy) { edit.index = proxy->mapToSource(edit.index); }
        }
    }
    history->push(new CSVEditCommand(this, sourceEdits, description));
}

QModelIndexList CSVEditor::getSelectedIndexes() const
{
    if(selectionModel() == nullptr) { return QModelIndexList{}; }
    return selectionModel()->selectedIndexes();
}

//選択したセルを、区切り文字で区切った表にする。各行は改行で終える（空の最後の行も残すため）
QString CSVEditor::getSelectedCellsAsText(QChar separator) const
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

    // 選択された範囲を特定（Ctrl+クリックなどの矩形でない選択もあるため、列は全インデックスから求める）
    int minRow = indexes.first().row();
    int maxRow = indexes.last().row();
    auto [minColIt, maxColIt] = std::minmax_element(indexes.begin(), indexes.end(), [](const QModelIndex& a, const QModelIndex& b) {
        return a.column() < b.column();
    });
    int minCol = minColIt->column();
    int maxCol = maxColIt->column();

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
            rowFields.append(FormatCell(cellText, separator));
        }

        result += rowFields.join(separator) + "\n";
    }

    return result;
}

void CSVEditor::setData(QModelIndex index, QVariant value, int role)
{
    //履歴のセルは元のモデルのもの（executeEditCommand）。表示していないファイルのセルも書き換えられる
    const_cast<QAbstractItemModel*>(index.model())->setData(index, value, role);
}

void CSVEditor::onHeaderSectionClicked(int logicalIndex)
{
    if(auto* proxy = dynamic_cast<CSVEditorSortFilterProxyModel*>(model())) {
        proxy->cycleColumnSort(logicalIndex);
    }
}

void CSVEditor::setFramedCell(const QModelIndex& sourceIndex)
{
    this->framedSourceIndex = sourceIndex;
    this->viewport()->update();
}

void CSVEditor::paintEvent(QPaintEvent* event)
{
    QTableView::paintEvent(event);
    if(this->framedSourceIndex.isValid() == false) { return; }

    auto* proxy = qobject_cast<QAbstractProxyModel*>(this->model());
    const auto* sourceModel = proxy ? proxy->sourceModel() : this->model();
    if(this->framedSourceIndex.model() != sourceModel) { return; }

    const auto index = proxy ? proxy->mapFromSource(this->framedSourceIndex) : QModelIndex(this->framedSourceIndex);
    //絞り込みで隠れている行は表示しない
    if(index.isValid() == false) { return; }

    QPainter painter(this->viewport());
    painter.setPen(QPen(QColor(0, 200, 83), 2));
    painter.setBrush(Qt::NoBrush);
    //スクロール時の残像対策
    painter.drawRect(this->visualRect(index).adjusted(1, 1, -1, -1));
}
