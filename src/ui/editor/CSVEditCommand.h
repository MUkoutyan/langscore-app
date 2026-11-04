#pragma once
#include <QUndoStack>
#include <QModelIndex>

struct CSVEditCommand : public QUndoCommand 
{
    struct CellEditsListener {
        virtual void setData(QModelIndex index, QVariant value, int role) = 0;
        //編集を反映した後に呼ぶ。変更したセルを表示するために使う
        virtual void cellEditsApplied(const QModelIndex& firstIndex) { Q_UNUSED(firstIndex); }
        bool _suppressUndoTracking = false;
        bool _isModified = false;
    };

    struct CellEdit {
        //並べ替え・絞り込みで行が移動しても同じセルを指すよう、永続インデックスで保持する。
        //表示ファイルの切り替えなどでモデルが変わった場合は無効になり、Undo/Redo の対象外になる。
        QPersistentModelIndex index;
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