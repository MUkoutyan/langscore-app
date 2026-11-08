#pragma once

#include <QTableView>
#include <QAction>
#include <QMenu>
#include <QClipboard>
#include <memory>
#include "ComponentBase.h"
#include "EditorTableDefines.h"
#include "EditDataManager.h"
#include "CSVEditCommand.h"
#include "CSVEditorSortFilterProxyModel.h"
#include "../dialog/TranslationProgressDialog.h"
#include "../../translation/TranslationManager.h"


namespace langscore {
    class CSVEditorTableModel;
}

class CSVEditor : public QTableView, public ComponentBase, public CSVEditCommand::CellEditsListener {
    Q_OBJECT

public:
    explicit CSVEditor(ComponentBase* component, QWidget* parent = nullptr);

    void setModel(QAbstractItemModel* model) override;
    using QTableView::edit;

    void setText(QModelIndex index, QString newText);

    // Edit operations
    void clearSelectedCells();

    // Clipboard operations
    void cut();
    void copy();
    void paste();

    // Undo/Redo
    void undo();
    void redo();

    // Selection operations
    void selectAll();

    //文字を打って編集を始めたところか。そのときは元の文章を置き換える
    bool isReplacingTextOnEdit() const { return _replaceTextOnEdit; }

    //from のセルから行・列方向に進んだ、次の編集できるセルを編集する
    void selectAndEditNextCell(const QModelIndex& from, int rowStep, int columnStep);
    void hideLanguageColumns();
    bool isLanguageColumnHidden(const QString& language) const;

    //テストプレイ中のゲームで表示している行を、選択とは別に緑の枠で表示する。
    //sourceIndex は並べ替え・絞り込みの前のモデルの位置。無効な位置なら枠を消す
    void setFramedCell(const QModelIndex& sourceIndex);
    QModelIndex framedCell() const { return this->framedSourceIndex; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool edit(const QModelIndex& index, EditTrigger trigger, QEvent* event) override;

    bool isEditableIndex(const QModelIndex& index);
    void cellEditsApplied(const QModelIndex& firstIndex) override;

private slots:
    void onCustomContextMenuRequested(const QPoint& pos);
    void openTranslationSettings();
    void translateSelectedCells();
    void retranslateSelectedCells();
    void onBatchTranslationCompleted(int batchId, const QList<TranslationManager::BatchTranslationResult>& results);
    void onBatchTranslationError(int batchId, const QString& errorMessage);
    void onTranslationProgress(int batchId, int completed, int total);
    void showAllColumns();
    void sortStateChanged(int col, SortOrder order);

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
    QList<QModelIndex> getTranslatableSelectedCells(bool includeTranslated) const;
    void translateCells(const QList<QModelIndex>& targetCells);
    QString getOriginalTextForRow(int row) const;
    QString getTargetLanguageForColumn(int column, TranslationService::ServiceType serviceType = TranslationService::ServiceType::DeepL) const;
    int findOriginalColumn() const;
    
    // Column visibility helper methods
    QStringList getRecognizedLanguageCodesInColumns() const;
    void changeColumnsVisibleWithLanguageCodes(const std::vector<std::pair<QString, bool>>& languageCodes);

    // Private helper methods
    void executeEditCommand(const QList<CSVEditCommand::CellEdit>& edits, const QString& description);
    QModelIndexList getSelectedIndexes() const;
    QString getSelectedCellsAsText(QChar separator) const;

    void setData(QModelIndex index, QVariant value, int role) override;

    bool _replaceTextOnEdit = false;
    QPersistentModelIndex framedSourceIndex;

    // Actions
    QAction* cutAction;
    QAction* copyAction;
    QAction* pasteAction;
    QAction* deleteAction;
    QAction* selectAllAction;
    QAction* translationSettingsAction;
    QAction* translateAction;
    QAction* retranslateAction;
    QAction* undoAction;
    QAction* redoAction;
    QAction* hideLanguageColumnsAction;
    QAction* showAllColumnsAction;
    
    // Menus
    QMenu* contextMenu;
    QMenu* translationMenu;
    QMenu* columnVisibilityMenu;
    
    // Translation
    std::unique_ptr<TranslationManager> translationManager;
    TranslationProgressDialog* progressDialog;
    // 翻訳要求時の (行, 列) → 書き込み先のセル。
    // 翻訳中に並べ替え・絞り込み・ファイル切り替えが行われても、正しいセルに書き込む（または破棄する）ため
    QMap<std::pair<int, int>, QPersistentModelIndex> pendingTranslationCells;

    void onHeaderSectionClicked(int logicalIndex);
};