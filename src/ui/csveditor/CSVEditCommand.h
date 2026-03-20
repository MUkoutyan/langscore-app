#pragma once
#include <QUndoStack>
#include <QModelIndex>

struct CSVEditCommand : public QUndoCommand 
{
    struct CellEditsListener {
        virtual void setData(QModelIndex index, QVariant value, int role) = 0;
        bool _suppressUndoTracking = false;
        bool _isModified = false;
    };

    struct CellEdit {
        QModelIndex index;
        QVariant oldValue;
        QVariant newValue;
    };

    CSVEditCommand(CellEditsListener* parent, const QList<CellEdit>& edits, const QString& description = QString())
        : parent(parent), edits(edits) {
        setText(description.isEmpty() ? QObject::tr("Edit Cells") : description);
    }

    int id() const override { return 7; }
    void undo() override;
    void redo() override;

private:
    CellEditsListener* parent;
    QList<CellEdit> edits;
    void applyCellEdits(bool isUndo);
};