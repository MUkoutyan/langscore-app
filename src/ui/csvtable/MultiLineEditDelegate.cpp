#include "MultiLineEditDelegate.h"
#include <QApplication>
#include <QFontMetrics>

MultiLineEditDelegate::MultiLineEditDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QWidget* MultiLineEditDelegate::createEditor(QWidget* parent, 
                                           const QStyleOptionViewItem& option, 
                                           const QModelIndex& index) const
{
    auto* editor = new MultiLineTextEdit(parent);
    editor->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    editor->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    
    connect(editor, &MultiLineTextEdit::editingFinished, 
            this, &MultiLineEditDelegate::commitAndCloseEditor);
    
    return editor;
}

void MultiLineEditDelegate::setEditorData(QWidget* editor, const QModelIndex& index) const
{
    auto* textEdit = qobject_cast<MultiLineTextEdit*>(editor);
    if (textEdit) {
        QString text = index.model()->data(index, Qt::EditRole).toString();
        textEdit->setPlainText(text);
    }
}

void MultiLineEditDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, 
                                       const QModelIndex& index) const
{
    auto* textEdit = qobject_cast<MultiLineTextEdit*>(editor);
    if (textEdit) {
        QString text = textEdit->toPlainText();
        model->setData(index, text, Qt::EditRole);
    }
}

void MultiLineEditDelegate::updateEditorGeometry(QWidget* editor, 
                                               const QStyleOptionViewItem& option, 
                                               const QModelIndex& index) const
{
    auto* textEdit = qobject_cast<MultiLineTextEdit*>(editor);
    if (textEdit) {
        QRect rect = option.rect;
        
        // テキストの内容に基づいて高さを調整
        QString text = index.model()->data(index, Qt::EditRole).toString();
        QFontMetrics fm(textEdit->font());
        int lineCount = text.split('\n').size();
        int minHeight = fm.lineSpacing() * qMax(3, lineCount) + 10; // 最低3行、余白10px
        
        rect.setHeight(qMax(rect.height(), minHeight));
        editor->setGeometry(rect);
    }
}

QSize MultiLineEditDelegate::sizeHint(const QStyleOptionViewItem& option, 
                                    const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    
    // テキストが改行を含む場合、高さを調整
    QString text = index.model()->data(index, Qt::DisplayRole).toString();
    if (text.contains('\n')) {
        QFontMetrics fm(option.font);
        int lineCount = text.split('\n').size();
        size.setHeight(fm.lineSpacing() * lineCount + 10);
    }
    
    return size;
}

void MultiLineEditDelegate::commitAndCloseEditor()
{
    auto* editor = qobject_cast<MultiLineTextEdit*>(sender());
    if (editor) {
        emit commitData(editor);
        emit closeEditor(editor);
    }
}

// MultiLineTextEdit implementation
MultiLineEditDelegate::MultiLineTextEdit::MultiLineTextEdit(QWidget* parent)
    : QTextEdit(parent)
{
    setTabChangesFocus(true);
}

void MultiLineEditDelegate::MultiLineTextEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Ctrl+Enter または Shift+Enter で編集完了
        if (event->modifiers() & (Qt::ControlModifier | Qt::ShiftModifier)) {
            emit editingFinished();
            return;
        }
        // 通常のEnterは改行
        QTextEdit::keyPressEvent(event);
    } else if (event->key() == Qt::Key_Tab) {
        // Tabで編集完了
        emit editingFinished();
    } else if (event->key() == Qt::Key_Escape) {
        // Escapeで編集キャンセル
        emit editingFinished();
    } else {
        QTextEdit::keyPressEvent(event);
    }
}

#include "MultiLineEditDelegate.moc"