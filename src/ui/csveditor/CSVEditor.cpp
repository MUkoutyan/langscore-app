#include "CSVEditor.h"
#include "CSVEditorTableModel.h"
#include "../csvtable/MultiLineEditDelegate.h"
#include "../TranslationApiSettingsDialog.h"
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
#include <QLabel>
#include "FastCSVContainer.h"

using namespace langscore;

// Custom dialog class for language column selection
class LanguageColumnSelectionDialog : public QDialog {
    Q_OBJECT

public:
    LanguageColumnSelectionDialog(const QStringList& languages, CSVEditor* csvEditor, QWidget* parent = nullptr)
        : QDialog(parent), selectedLanguages(), csvEditor(csvEditor)
    {
        setWindowTitle(tr("Hide Language Columns"));
        setModal(true);
        resize(300, 400);
        
        setupUI(languages);
    }
    
    std::vector<std::pair<QString, bool>> getLanguagesVisibleState() const {
        std::vector<std::pair<QString, bool>> result;
        for (const auto& [lang, checkBox] : checkBoxes.asKeyValueRange()) {
            result.emplace_back(std::pair(lang, checkBox->isChecked()));
        }
        return result;
    }

private slots:
    void onSelectAll() {
        for (auto* checkBox : checkBoxes) {
            checkBox->setChecked(true);
        }
    }
    
    void onDeselectAll() {
        for (auto* checkBox : checkBoxes) {
            checkBox->setChecked(false);
        }
    }

private:
    void setupUI(const QStringList& languages) {
        auto* mainLayout = new QVBoxLayout(this);
        
        // Title label
        auto* titleLabel = new QLabel(tr("Select language columns to hide:"), this);
        titleLabel->setWordWrap(true);
        mainLayout->addWidget(titleLabel);
        
        // Checkbox area with scroll if needed
        auto* scrollArea = new QScrollArea(this);
        auto* scrollWidget = new QWidget();
        auto* scrollLayout = new QVBoxLayout(scrollWidget);
        
        // Create checkboxes for each language
        for (const QString& language : languages) {
            auto* checkBox = new QCheckBox(language, scrollWidget);
            
            // Set initial state based on current column visibility
            bool isCurrentlyVisible = isLanguageColumnHidden(language);
            checkBox->setChecked(isCurrentlyVisible);
            
            checkBoxes[language] = checkBox;
            scrollLayout->addWidget(checkBox);
        }
        
        scrollLayout->addStretch();
        scrollArea->setWidget(scrollWidget);
        scrollArea->setWidgetResizable(true);
        scrollArea->setMaximumHeight(250);
        mainLayout->addWidget(scrollArea);
        
        // Selection buttons
        auto* selectionLayout = new QHBoxLayout();
        auto* selectAllBtn = new QPushButton(tr("Select All"), this);
        auto* deselectAllBtn = new QPushButton(tr("Deselect All"), this);
        
        connect(selectAllBtn, &QPushButton::clicked, this, &LanguageColumnSelectionDialog::onSelectAll);
        connect(deselectAllBtn, &QPushButton::clicked, this, &LanguageColumnSelectionDialog::onDeselectAll);
        
        selectionLayout->addWidget(selectAllBtn);
        selectionLayout->addWidget(deselectAllBtn);
        selectionLayout->addStretch();
        mainLayout->addLayout(selectionLayout);
        
        // Dialog buttons
        auto* buttonLayout = new QHBoxLayout();
        auto* okButton = new QPushButton(tr("OK"), this);
        auto* cancelButton = new QPushButton(tr("Cancel"), this);
        
        connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
        
        buttonLayout->addStretch();
        buttonLayout->addWidget(okButton);
        buttonLayout->addWidget(cancelButton);
        mainLayout->addLayout(buttonLayout);
    }
    
    bool isLanguageColumnHidden(const QString& language) const {
        if(!csvEditor || !csvEditor->model()) { return false; }
        
        for (int col = 0; col < csvEditor->model()->columnCount(); ++col) {
            QString headerText = csvEditor->model()->headerData(col, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
            
            if (headerText == language.toLower()) {
                return csvEditor->isColumnHidden(col) == false;
            }
        }
        return false;
    }
    
    QMap<QString, QCheckBox*> checkBoxes;
    QStringList selectedLanguages;
    CSVEditor* csvEditor;
};

// CSVEditCommand implementation
void CSVEditor::CSVEditCommand::undo() {
    applyCellEdits(true);
}

void CSVEditor::CSVEditCommand::redo() {
    applyCellEdits(false);
}

void CSVEditor::CSVEditCommand::applyCellEdits(bool isUndo) {
    if (!parent || !parent->model()) return;

    parent->_suppressUndoTracking = true;
    
    for (const auto& edit : edits) {
        if (edit.index.isValid()) {
            QVariant value = isUndo ? edit.oldValue : edit.newValue;
            parent->model()->setData(edit.index, value, Qt::EditRole);
        }
    }
    
    parent->_suppressUndoTracking = false;
    parent->_isModified = true;
}

// CSVEditor implementation
CSVEditor::CSVEditor(std::weak_ptr<CSVEditDataManager> loadFileManager, ComponentBase* component, QWidget* parent)
    : QTableView(parent)
    , ComponentBase(component)
    , loadFileManager(loadFileManager)
    , contextMenu(nullptr)
    , translationMenu(nullptr)
    , columnVisibilityMenu(nullptr)
    , translationManager(std::make_unique<TranslationManager>(this))
    , progressDialog(nullptr)
    , _isModified(false)
    , _suppressUndoTracking(false)
{
    this->setSelectionBehavior(QAbstractItemView::SelectItems);
    this->setSelectionMode(QAbstractItemView::ExtendedSelection);
    this->setContextMenuPolicy(Qt::CustomContextMenu);

    // マルチライン編集デリゲートを設定
    auto* multiLineDelegate = new MultiLineEditDelegate(this);
    this->setItemDelegate(multiLineDelegate);
    
    setupActions();
    setupContextMenu();
    
    connect(this, &QWidget::customContextMenuRequested, 
            this, &CSVEditor::onCustomContextMenuRequested);
            
    // Translation manager connections
    connect(translationManager.get(), &TranslationManager::batchTranslationCompleted,
            this, &CSVEditor::onBatchTranslationCompleted);
    connect(translationManager.get(), &TranslationManager::batchTranslationError,
            this, &CSVEditor::onBatchTranslationError);
    connect(translationManager.get(), &TranslationManager::translationProgress,
            this, &CSVEditor::onTranslationProgress);
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
    hideLanguageColumnsAction->setEnabled(!recognizedLanguages.isEmpty());
    
    // Check if any columns are hidden
    bool hasHiddenColumns = false;
    for (int i = 0; i < model()->columnCount(); ++i) {
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
    if (!model()) return;
    
    QStringList recognizedLanguages = getRecognizedLanguageCodesInColumns();
    
    if (recognizedLanguages.isEmpty()) {
        QMessageBox::information(this, tr("Column Visibility"), 
                               tr("No recognizable language columns found."));
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
    if (!model()) return;
    
    // Show all columns
    for (int i = 0; i < model()->columnCount(); ++i) {
        setColumnHidden(i, false);
    }
    
    updateColumnVisibilityMenu();
}

QStringList CSVEditor::getRecognizedLanguageCodesInColumns() const
{
    if(!model()) { return QStringList(); }
    

    QStringList systemColumnName = {
        tr("original"), tr("type")
    };
    
    // Check each column header
    QStringList recognizedLanguages;
    for (int col = 0; col < model()->columnCount(); ++col) {
        QString headerText = model()->headerData(col, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
        
        if (systemColumnName.contains(headerText) == false) {
            recognizedLanguages.append(headerText);
        }
    }
    
    recognizedLanguages.removeDuplicates();
    return recognizedLanguages;
}

//void CSVEditor::hideColumnsWithLanguageCodes(const std::vector<std::pair<QString, bool>>& languageCodes)
//{
//    if(!model()) { return; }
//    
//    // Hide columns that match the specified language codes
//    for (int col = 0; col < model()->columnCount(); ++col) {
//        QString headerText = model()->headerData(col, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
//        
//        if(std::ranges::find_if(languageCodes, [&headerText](const auto& p) { return p.first == headerText; }) != languageCodes.end()) {
//            setColumnHidden(col, true);
//        }
//    }
//}

void CSVEditor::changeColumnsVisibleWithLanguageCodes(const std::vector<std::pair<QString, bool>>& languageCodes)
{
    if(!model()) { return; }
    
    // Show columns that match the specified language codes
    for (int col = 0; col < model()->columnCount(); ++col) 
    {
        QString headerText = model()->headerData(col, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();

        auto find_result = std::ranges::find_if(languageCodes, [&headerText](const auto& p) { return p.first == headerText; });
        if(find_result != languageCodes.end()) {
            setColumnHidden(col, find_result->second == false);
        }
    }
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
                           tr("Could not find 'Original' column for source text."));
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
    
    for (const QModelIndex& cellIndex : emptyCells) {
        QString originalText = getOriginalTextForRow(cellIndex.row());
        QString targetLang = getTargetLanguageForColumn(cellIndex.column(), preferredService);
        
        if (originalText.isEmpty()) {
            continue; // Skip rows without original text
        }
        
        if (targetLang.isEmpty()) {
            continue; // Skip columns without recognizable language
        }
        
        TranslationManager::BatchTranslationRequest request;
        request.text = originalText;
        request.sourceLang = "ja"; // Assume Japanese source, can be made configurable
        request.targetLang = targetLang;
        request.row = cellIndex.row();
        request.column = cellIndex.column();
        
        requests.append(request);
    }
    
    if (requests.isEmpty()) {
        QMessageBox::warning(this, tr("Translation Error"), 
                           tr("No valid translation requests could be prepared."));
        return;
    }
    
    // Show progress dialog
    if (!progressDialog) {
        progressDialog = new TranslationProgressDialog(this);
        connect(progressDialog, &TranslationProgressDialog::cancelled,
                this, [this]() {
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
    
    // Get header text
    QString headerText = model()->headerData(column, Qt::Horizontal, Qt::UserRole).toString().toLower().trimmed();
    
    // Define language mappings for different services
    static const QMap<QString, QMap<TranslationService::ServiceType, QString>> langServiceMap = {
        // English
        {"english", {{TranslationService::ServiceType::DeepL, "EN"}, {TranslationService::ServiceType::GoogleTranslate, "en"}}},
        {"en", {{TranslationService::ServiceType::DeepL, "EN"}, {TranslationService::ServiceType::GoogleTranslate, "en"}}},
        
        // Japanese
        {"japanese", {{TranslationService::ServiceType::DeepL, "JA"}, {TranslationService::ServiceType::GoogleTranslate, "ja"}}},
        {"jp", {{TranslationService::ServiceType::DeepL, "JA"}, {TranslationService::ServiceType::GoogleTranslate, "ja"}}},
        {"ja", {{TranslationService::ServiceType::DeepL, "JA"}, {TranslationService::ServiceType::GoogleTranslate, "ja"}}},
        
        // Chinese (Simplified)
        {"chinese", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        {"zh", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        {"cn", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        {"zh-cn", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-CN"}}},
        {"zh_cn", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-CN"}}},
        {"simplified chinese", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        
        // Chinese (Traditional)
        {"traditional chinese", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-TW"}}},
        {"zh-tw", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-TW"}}},
        {"zh_tw", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-TW"}}},
        
        // German
        {"german", {{TranslationService::ServiceType::DeepL, "DE"}, {TranslationService::ServiceType::GoogleTranslate, "de"}}},
        {"de", {{TranslationService::ServiceType::DeepL, "DE"}, {TranslationService::ServiceType::GoogleTranslate, "de"}}},
        
        // French
        {"french", {{TranslationService::ServiceType::DeepL, "FR"}, {TranslationService::ServiceType::GoogleTranslate, "fr"}}},
        {"fr", {{TranslationService::ServiceType::DeepL, "FR"}, {TranslationService::ServiceType::GoogleTranslate, "fr"}}},
        
        // Spanish
        {"spanish", {{TranslationService::ServiceType::DeepL, "ES"}, {TranslationService::ServiceType::GoogleTranslate, "es"}}},
        {"es", {{TranslationService::ServiceType::DeepL, "ES"}, {TranslationService::ServiceType::GoogleTranslate, "es"}}},
        
        // Italian
        {"italian", {{TranslationService::ServiceType::DeepL, "IT"}, {TranslationService::ServiceType::GoogleTranslate, "it"}}},
        {"it", {{TranslationService::ServiceType::DeepL, "IT"}, {TranslationService::ServiceType::GoogleTranslate, "it"}}},
        
        // Portuguese
        {"portuguese", {{TranslationService::ServiceType::DeepL, "PT"}, {TranslationService::ServiceType::GoogleTranslate, "pt"}}},
        {"pt", {{TranslationService::ServiceType::DeepL, "PT"}, {TranslationService::ServiceType::GoogleTranslate, "pt"}}},
        
        // Russian
        {"russian", {{TranslationService::ServiceType::DeepL, "RU"}, {TranslationService::ServiceType::GoogleTranslate, "ru"}}},
        {"ru", {{TranslationService::ServiceType::DeepL, "RU"}, {TranslationService::ServiceType::GoogleTranslate, "ru"}}},
        
        // Korean
        {"korean", {{TranslationService::ServiceType::DeepL, "KO"}, {TranslationService::ServiceType::GoogleTranslate, "ko"}}},
        {"ko", {{TranslationService::ServiceType::DeepL, "KO"}, {TranslationService::ServiceType::GoogleTranslate, "ko"}}},
        
        // Dutch
        {"dutch", {{TranslationService::ServiceType::DeepL, "NL"}, {TranslationService::ServiceType::GoogleTranslate, "nl"}}},
        {"nl", {{TranslationService::ServiceType::DeepL, "NL"}, {TranslationService::ServiceType::GoogleTranslate, "nl"}}},
        
        // Polish
        {"polish", {{TranslationService::ServiceType::DeepL, "PL"}, {TranslationService::ServiceType::GoogleTranslate, "pl"}}},
        {"pl", {{TranslationService::ServiceType::DeepL, "PL"}, {TranslationService::ServiceType::GoogleTranslate, "pl"}}},
        
        // Swedish
        {"swedish", {{TranslationService::ServiceType::DeepL, "SV"}, {TranslationService::ServiceType::GoogleTranslate, "sv"}}},
        {"sv", {{TranslationService::ServiceType::DeepL, "SV"}, {TranslationService::ServiceType::GoogleTranslate, "sv"}}},
        
        // Danish
        {"danish", {{TranslationService::ServiceType::DeepL, "DA"}, {TranslationService::ServiceType::GoogleTranslate, "da"}}},
        {"da", {{TranslationService::ServiceType::DeepL, "DA"}, {TranslationService::ServiceType::GoogleTranslate, "da"}}},
        
        // Norwegian
        {"norwegian", {{TranslationService::ServiceType::DeepL, "NB"}, {TranslationService::ServiceType::GoogleTranslate, "no"}}},
        {"no", {{TranslationService::ServiceType::DeepL, "NB"}, {TranslationService::ServiceType::GoogleTranslate, "no"}}},
        {"nb", {{TranslationService::ServiceType::DeepL, "NB"}, {TranslationService::ServiceType::GoogleTranslate, "no"}}},
        
        // Finnish
        {"finnish", {{TranslationService::ServiceType::DeepL, "FI"}, {TranslationService::ServiceType::GoogleTranslate, "fi"}}},
        {"fi", {{TranslationService::ServiceType::DeepL, "FI"}, {TranslationService::ServiceType::GoogleTranslate, "fi"}}},
        
        // Czech
        {"czech", {{TranslationService::ServiceType::DeepL, "CS"}, {TranslationService::ServiceType::GoogleTranslate, "cs"}}},
        {"cs", {{TranslationService::ServiceType::DeepL, "CS"}, {TranslationService::ServiceType::GoogleTranslate, "cs"}}},
        
        // Hungarian
        {"hungarian", {{TranslationService::ServiceType::DeepL, "HU"}, {TranslationService::ServiceType::GoogleTranslate, "hu"}}},
        {"hu", {{TranslationService::ServiceType::DeepL, "HU"}, {TranslationService::ServiceType::GoogleTranslate, "hu"}}},
        
        // Romanian
        {"romanian", {{TranslationService::ServiceType::DeepL, "RO"}, {TranslationService::ServiceType::GoogleTranslate, "ro"}}},
        {"ro", {{TranslationService::ServiceType::DeepL, "RO"}, {TranslationService::ServiceType::GoogleTranslate, "ro"}}},
        
        // Slovak
        {"slovak", {{TranslationService::ServiceType::DeepL, "SK"}, {TranslationService::ServiceType::GoogleTranslate, "sk"}}},
        {"sk", {{TranslationService::ServiceType::DeepL, "SK"}, {TranslationService::ServiceType::GoogleTranslate, "sk"}}},
        
        // Slovenian
        {"slovenian", {{TranslationService::ServiceType::DeepL, "SL"}, {TranslationService::ServiceType::GoogleTranslate, "sl"}}},
        {"sl", {{TranslationService::ServiceType::DeepL, "SL"}, {TranslationService::ServiceType::GoogleTranslate, "sl"}}},
        
        // Bulgarian
        {"bulgarian", {{TranslationService::ServiceType::DeepL, "BG"}, {TranslationService::ServiceType::GoogleTranslate, "bg"}}},
        {"bg", {{TranslationService::ServiceType::DeepL, "BG"}, {TranslationService::ServiceType::GoogleTranslate, "bg"}}},
        
        // Estonian
        {"estonian", {{TranslationService::ServiceType::DeepL, "ET"}, {TranslationService::ServiceType::GoogleTranslate, "et"}}},
        {"et", {{TranslationService::ServiceType::DeepL, "ET"}, {TranslationService::ServiceType::GoogleTranslate, "et"}}},
        
        // Latvian
        {"latvian", {{TranslationService::ServiceType::DeepL, "LV"}, {TranslationService::ServiceType::GoogleTranslate, "lv"}}},
        {"lv", {{TranslationService::ServiceType::DeepL, "LV"}, {TranslationService::ServiceType::GoogleTranslate, "lv"}}},
        
        // Lithuanian
        {"lithuanian", {{TranslationService::ServiceType::DeepL, "LT"}, {TranslationService::ServiceType::GoogleTranslate, "lt"}}},
        {"lt", {{TranslationService::ServiceType::DeepL, "LT"}, {TranslationService::ServiceType::GoogleTranslate, "lt"}}},
        
        // Ukrainian
        {"ukrainian", {{TranslationService::ServiceType::DeepL, "UK"}, {TranslationService::ServiceType::GoogleTranslate, "uk"}}},
        {"uk", {{TranslationService::ServiceType::DeepL, "UK"}, {TranslationService::ServiceType::GoogleTranslate, "uk"}}},
        
        // Turkish
        {"turkish", {{TranslationService::ServiceType::DeepL, "TR"}, {TranslationService::ServiceType::GoogleTranslate, "tr"}}},
        {"tr", {{TranslationService::ServiceType::DeepL, "TR"}, {TranslationService::ServiceType::GoogleTranslate, "tr"}}},
        
        // Greek
        {"greek", {{TranslationService::ServiceType::DeepL, "EL"}, {TranslationService::ServiceType::GoogleTranslate, "el"}}},
        {"el", {{TranslationService::ServiceType::DeepL, "EL"}, {TranslationService::ServiceType::GoogleTranslate, "el"}}},
        
        // Arabic
        {"arabic", {{TranslationService::ServiceType::DeepL, "AR"}, {TranslationService::ServiceType::GoogleTranslate, "ar"}}},
        {"ar", {{TranslationService::ServiceType::DeepL, "AR"}, {TranslationService::ServiceType::GoogleTranslate, "ar"}}},
        
        // Hindi
        {"hindi", {{TranslationService::ServiceType::DeepL, "HI"}, {TranslationService::ServiceType::GoogleTranslate, "hi"}}},
        {"hi", {{TranslationService::ServiceType::DeepL, "HI"}, {TranslationService::ServiceType::GoogleTranslate, "hi"}}},
        
        // Indonesian
        {"indonesian", {{TranslationService::ServiceType::DeepL, "ID"}, {TranslationService::ServiceType::GoogleTranslate, "id"}}},
        {"id", {{TranslationService::ServiceType::DeepL, "ID"}, {TranslationService::ServiceType::GoogleTranslate, "id"}}},
        
        // Malay
        {"malay", {{TranslationService::ServiceType::DeepL, "MS"}, {TranslationService::ServiceType::GoogleTranslate, "ms"}}},
        {"ms", {{TranslationService::ServiceType::DeepL, "MS"}, {TranslationService::ServiceType::GoogleTranslate, "ms"}}},
        
        // Thai
        {"thai", {{TranslationService::ServiceType::DeepL, "TH"}, {TranslationService::ServiceType::GoogleTranslate, "th"}}},
        {"th", {{TranslationService::ServiceType::DeepL, "TH"}, {TranslationService::ServiceType::GoogleTranslate, "th"}}},
        
        // Vietnamese
        {"vietnamese", {{TranslationService::ServiceType::DeepL, "VI"}, {TranslationService::ServiceType::GoogleTranslate, "vi"}}},
        {"vi", {{TranslationService::ServiceType::DeepL, "VI"}, {TranslationService::ServiceType::GoogleTranslate, "vi"}}}
    };
    
    // Look up the language mapping for the header text
    if (langServiceMap.contains(headerText)) {
        const auto& serviceMap = langServiceMap[headerText];
        if (serviceMap.contains(serviceType)) {
            return serviceMap[serviceType];
        }
    }
    
    return QString(); // Return empty string if language not found
}

int CSVEditor::findOriginalColumn() const
{
    return 0;
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
    if (model()) {
        connect(model(), &QAbstractItemModel::dataChanged, 
                this, &CSVEditor::onModelDataChanged);
    }
}

void CSVEditor::onModelDataChanged()
{
    if (!_suppressUndoTracking) {
        _isModified = true;
    }
    updateTranslationMenu(); // Update menu when data changes
    updateColumnVisibilityMenu(); // Update column visibility menu when data changes
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

// CSV file operations
bool CSVEditor::openCSV(const QString& filePath, langscore::CSVEditorTableModel* csvModel)
{
    if (csvModel->rowCount() == 0 && csvModel->columnCount() == 0) {
        delete csvModel;
        return false;
    }
    
    setModel(csvModel);
    connectModelSignals();
    currentFilePath = filePath;
    _isModified = false;
    
    // Use ComponentBase history
    if (history) {
        history->clear();
    }
    
    return true;
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

void CSVEditor::newCSV(langscore::CSVEditorTableModel* csvModel)
{
    // 翻訳CSVの基本構造: 原文、翻訳文の列を作成
    csvModel->insertRows(0, 1);  // ヘッダー行
    csvModel->insertColumns(0, 2);  // 原文、翻訳文の列
    
    // ヘッダーを設定
    csvModel->setData(csvModel->index(0, 0), tr("Original"), Qt::EditRole);
    csvModel->setData(csvModel->index(0, 1), tr("Translation"), Qt::EditRole);
    
    setModel(csvModel);
    connectModelSignals();
    currentFilePath.clear();
    _isModified = false;
    
    // Use ComponentBase history
    if (history) {
        history->clear();
    }
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

    if (!edits.isEmpty()) {
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

// Include the MOC file for the LanguageColumnSelectionDialog class
#include "CSVEditor.moc"
