#pragma once
#include <QUndoStack>
#include <QModelIndex>

struct CSVEditCommand : public QUndoCommand 
{
    struct CellEditsListener {
        virtual void setData(QModelIndex index, QVariant value, int role) = 0;
        //編集を反映した後に呼ぶ。変更したセルを表示するために使う
        virtual void cellEditsApplied(const QModelIndex& firstIndex) { Q_UNUSED(firstIndex); }
    };

    struct CellEdit {
        //並べ替え・絞り込みで行が移動しても同じセルを指すよう、永続インデックスで保持する。
        //CSVEditor は表示用の proxy ではなく元のモデルのセルを入れるため、ファイルの切り替えや列を隠した後も有効のまま。
        //言語の列の追加などでモデルを再構築して無効になった場合は、Undo/Redo の対象外になる。
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
    bool notifyApplied = false;
    void applyCellEdits(bool isUndo);
};