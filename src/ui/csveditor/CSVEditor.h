#pragma once

#include <QTableView>
#include <QAction>
#include <QMenu>
#include <QUndoStack>
#include <QClipboard>
#include <memory>
#include "ComponentBase.h"
#include "CSVEditDataManager.h"
#include "../TranslationProgressDialog.h"
#include "../../translation/TranslationManager.h"


namespace langscore {
    class CSVEditorTableModel;
}

class CSVEditor : public QTableView, public ComponentBase {
    Q_OBJECT

public:
    struct CSVEditCommand : public QUndoCommand {
        struct CellEdit {
            QModelIndex index;
            QVariant oldValue;
            QVariant newValue;
        };

        CSVEditCommand(CSVEditor* parent, const QList<CellEdit>& edits, const QString& description = QString())
            : parent(parent), edits(edits) {
            setText(description.isEmpty() ? QObject::tr("Edit Cells") : description);
        }

        int id() const override { return 7; }
        void undo() override;
        void redo() override;

    private:
        CSVEditor* parent;
        QList<CellEdit> edits;
        void applyCellEdits(bool isUndo);
    };

    explicit CSVEditor(std::weak_ptr<CSVEditDataManager> loadFileManager, ComponentBase* component, QWidget* parent = nullptr);

    bool openCSV(const QString& filePath, langscore::CSVEditorTableModel* csvModel);
    bool saveCSV(const QString& filePath = QString(), langscore::CSVEditorTableModel* csvModel = nullptr);
    bool saveAsCSV(const QString& filePath = QString(), langscore::CSVEditorTableModel* csvModel = nullptr);
    void newCSV(langscore::CSVEditorTableModel* csvModel);

    // Edit operations
    void clearSelectedCells();

    // Clipboard operations
    void cut();
    void copy();
    void paste();

    // Undo/Redo
    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    // Selection operations
    void selectAll();

    bool isModified() const { return _isModified; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onCustomContextMenuRequested(const QPoint& pos);
    void onModelDataChanged();
    void openTranslationSettings();
    void translateSelectedCells();
    void onBatchTranslationCompleted(int batchId, const QList<TranslationManager::BatchTranslationResult>& results);
    void onBatchTranslationError(int batchId, const QString& errorMessage);
    void onTranslationProgress(int batchId, int completed, int total);
    void hideLanguageColumns();
    void showAllColumns();

private:
    void setupActions();
    void setupContextMenu();
    void connectModelSignals();
    void showContextMenu(const QPoint& globalPos);
    void updateTranslationMenu();
    void updateColumnVisibilityMenu();
    
    // Translation helper methods
    bool hasConfiguredTranslationService() const;
    QList<QModelIndex> getEmptySelectedCells() const;
    QString getOriginalTextForRow(int row) const;
    QString getTargetLanguageForColumn(int column, TranslationService::ServiceType serviceType = TranslationService::ServiceType::DeepL) const;
    int findOriginalColumn() const;
    
    // Column visibility helper methods
    QStringList getRecognizedLanguageCodesInColumns() const;
    //void hideColumnsWithLanguageCodes(const std::vector<std::pair<QString, bool>>& languageCodes);
    void changeColumnsVisibleWithLanguageCodes(const std::vector<std::pair<QString, bool>>& languageCodes);
    
    // Private helper methods
    void executeEditCommand(const QList<CSVEditCommand::CellEdit>& edits, const QString& description);
    QModelIndexList getSelectedIndexes() const;
    QString formatCSVField(const QString& field) const;
    QStringList parseCSVLine(const QString& line) const;
    QStringList parseCSVRows(const QString& text) const;
    QString getSelectedCellsAsText() const;

    std::weak_ptr<CSVEditDataManager> loadFileManager;
    
    // Actions
    QAction* cutAction;
    QAction* copyAction;
    QAction* pasteAction;
    QAction* deleteAction;
    QAction* selectAllAction;
    QAction* translationSettingsAction;
    QAction* translateAction;
    QAction* hideLanguageColumnsAction;
    QAction* showAllColumnsAction;
    
    // Menus
    QMenu* contextMenu;
    QMenu* translationMenu;
    QMenu* columnVisibilityMenu;
    
    // Translation
    std::unique_ptr<TranslationManager> translationManager;
    TranslationProgressDialog* progressDialog;
    
    QString currentFilePath;
    bool _isModified;
    bool _suppressUndoTracking;
};