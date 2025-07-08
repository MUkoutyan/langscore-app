#include "CSVEditor.h"
#include "MainCSVTableModel.h"
#include "MultiLineEditDelegate.h"
#include <QVBoxLayout>
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
#include "FastCSVContainer.h"

using namespace langscore;

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
    
    // アクションを追加
    addAction(cutAction);
    addAction(copyAction);
    addAction(pasteAction);
    addAction(deleteAction);
    addAction(selectAllAction);
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

        contextMenu->exec(globalPos);
    }
}

// CSV file operations
bool CSVEditor::openCSV(const QString& filePath, CSVEditorTableModel* csvModel)
{
    //auto* csvModel = new langscore::CSVEditorTableModel(filePath, this);
    if (csvModel->rowCount() == 0 && csvModel->columnCount() == 0) {
        delete csvModel;
        return false;
    }
    
    setModel(csvModel);
    connectModelSignals();
    currentFilePath = filePath;
    _isModified = false;
    history->clear();
    
    return true;
}

bool CSVEditor::saveCSV(const QString& filePath, CSVEditorTableModel* csvModel)
{
    //auto* csvModel = qobject_cast<langscore::CSVEditorTableModel*>(model());
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

bool CSVEditor::saveAsCSV(const QString& filePath, CSVEditorTableModel* csvModel)
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

void CSVEditor::newCSV(CSVEditorTableModel* csvModel)
{
    //auto* csvModel = new langscore::CSVEditorTableModel(this);
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
    history->clear();
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
    history->undo();
}

void CSVEditor::redo()
{
    history->redo();
}

bool CSVEditor::canUndo() const
{
    return history->canUndo();
}

bool CSVEditor::canRedo() const
{
    return history->canRedo();
}

// Selection operations
void CSVEditor::selectAll()
{
    QTableView::selectAll();
}

// Private helper methods
void CSVEditor::executeEditCommand(const QList<CSVEditCommand::CellEdit>& edits, const QString& description)
{
    if (edits.isEmpty()) return;
    
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
