#pragma once

#include <QTableView>
#include <QAbstractTableModel>
#include <QStringList>
#include <QKeyEvent>
#include <QUndoStack>
#include <QUndoCommand>
#include <QMenu>
#include <QAction>
#include <vector>
#include "CSVEditorTable.h"
#include "FastCSVContainer.h"
#include "../ComponentBase.h"

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

    explicit CSVEditor(ComponentBase* component, QWidget* parent = nullptr);

    // CSV file operations
    bool openCSV(const QString& filePath);
    bool saveCSV(const QString& filePath);
    bool saveAsCSV(const QString& filePath);
    void newCSV();

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

    // Modification state
    bool isModified() const { return _isModified; }

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    void onCustomContextMenuRequested(const QPoint& pos);
    void onModelDataChanged();

private:
    void setupActions();
    void setupContextMenu();
    void connectModelSignals();
    QModelIndexList getSelectedIndexes() const;
    QString formatCSVField(const QString& field) const;
    QStringList parseCSVLine(const QString& line) const;
    QStringList parseCSVRows(const QString& text) const;
    QString getSelectedCellsAsText() const;
    void executeEditCommand(const QList<CSVEditCommand::CellEdit>& edits, const QString& description);

    // Context menu
    QMenu* contextMenu;
    QAction* cutAction;
    QAction* copyAction;
    QAction* pasteAction;
    QAction* deleteAction;
    QAction* selectAllAction;

    // Undo/Redo
    QUndoStack* undoStack;

    // Current file path
    QString currentFilePath;
    bool _isModified;
    bool _suppressUndoTracking;
};