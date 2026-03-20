#include "CSVEditCommand.h"


// CSVEditCommand implementation
void CSVEditCommand::undo() {
    applyCellEdits(true);
}

void CSVEditCommand::redo() {
    applyCellEdits(false);
}

void CSVEditCommand::applyCellEdits(bool isUndo) {
    if(!parent) return;

    parent->_suppressUndoTracking = true;

    for(const auto& edit : edits) {
        if(edit.index.isValid()) {
            QVariant value = isUndo ? edit.oldValue : edit.newValue;
            parent->setData(edit.index, value, Qt::EditRole);
        }
    }

    parent->_suppressUndoTracking = false;
    parent->_isModified = true;
}