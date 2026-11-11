#include "CSVEditCommand.h"
#include <algorithm>
#include <utility>


// CSVEditCommand implementation
void CSVEditCommand::undo() {
    applyCellEdits(true);
}

void CSVEditCommand::redo() {
    applyCellEdits(false);
}

void CSVEditCommand::applyCellEdits(bool isUndo) {
    if(!parent) return;

    for(const auto& edit : edits) {
        if(edit.index.isValid()) {
            QVariant value = isUndo ? edit.oldValue : edit.newValue;
            parent->setData(edit.index, value, Qt::EditRole);
        }
    }

    //編集時の最初の実行では、編集したセルが表示中のため通知しない
    if(std::exchange(notifyApplied, true) == false) { return; }
    const auto firstValid = std::ranges::find_if(edits, [](const auto& edit) { return edit.index.isValid(); });
    if(firstValid != edits.end()) {
        parent->cellEditsApplied(firstValid->index);
    }
}