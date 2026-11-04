#include "MultiLineEditDelegate.h"
#include "CSVEditor.h"
#include <QApplication>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QLabel>


static CSVEditor* getCSVEditor(const QObject* obj)
{
    auto parent = obj->parent();
    while(parent) {
        if(auto editor = qobject_cast<CSVEditor*>(parent)) {
            return editor;
        }
        parent = parent->parent();
    }
    return nullptr;
}

class EditorWidgetWrapper : public QWidget
{
    Q_OBJECT
public:
    EditorWidgetWrapper(MultiLineEditDelegate* delegate, QWidget* parent)
        :QWidget(parent)
    {
        // レイアウトの余白を 0 に設定してセルとのズレを無くす
        auto* containerLayout = new QVBoxLayout(this);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);

        // テキストエディタの作成
        textEdit = new MultiLineEditDelegate::MultiLineTextEdit(this);
        textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        textEdit->setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

        // エディタ自体のフレームを消す（セルの枠線と重複して太く見えるのを防ぐ）
        textEdit->setFrameStyle(QFrame::NoFrame);

        // ガイドラベルの作成
        auto* guideLabel = new QLabel(tr("Ctrl+Enter: Confirm  Shift+Enter: Confirm and move down  Tab: Confirm and move right  Esc: Cancel"), this);
        guideLabel->setStyleSheet("color: white; font-size: 8pt; background-color: #505050;");
        guideLabel->setAlignment(Qt::AlignRight);
        // ラベルがエディタを圧迫しないよう高さを固定
        guideLabel->setFixedHeight(18);

        containerLayout->addWidget(textEdit);
        containerLayout->addWidget(guideLabel);

        this->setFocusProxy(textEdit);


        connect(textEdit, &MultiLineEditDelegate::MultiLineTextEdit::editingFinished, [delegate, this](MultiLineEditDelegate::MultiLineTextEdit::MoveTo moveTo)
        {
            emit delegate->commitData(this);
            emit delegate->closeEditor(this);

            if(moveTo == MultiLineEditDelegate::MultiLineTextEdit::MoveTo::None) { return; }
            if(auto editor = getCSVEditor(this))
            {
                const bool moveDown = (moveTo == MultiLineEditDelegate::MultiLineTextEdit::MoveTo::Down);
                editor->selectAndEditNextCell(moveDown ? 1 : 0, moveDown ? 0 : 1);
            }
        });

        connect(textEdit, &MultiLineEditDelegate::MultiLineTextEdit::editingCanceled, [delegate, this]() {
            emit delegate->closeEditor(this, QAbstractItemDelegate::RevertModelCache);
        });
    }

    MultiLineEditDelegate::MultiLineTextEdit* getTextEdit() const { return textEdit; }

private:

    MultiLineEditDelegate::MultiLineTextEdit* textEdit;

};

MultiLineEditDelegate::MultiLineEditDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QWidget* MultiLineEditDelegate::createEditor(QWidget* parent, 
                                           const QStyleOptionViewItem& option, 
                                           const QModelIndex& index) const
{
    return new EditorWidgetWrapper(const_cast<MultiLineEditDelegate*>(this), parent);
}

void MultiLineEditDelegate::setEditorData(QWidget* _editor, const QModelIndex& index) const
{
    auto* editor = qobject_cast<EditorWidgetWrapper*>(_editor);
    if (editor) {
        auto textEdit = editor->getTextEdit();
        QString text = index.model()->data(index, Qt::EditRole).toString();
        auto font_var = index.model()->data(index, Qt::FontRole);
        if(font_var.isValid()) {
            auto font = font_var.value<QFont>();
            textEdit->setFont(font);
        }
        
        textEdit->setPlainText(text);
        //続きを書き足せるよう、カーソルを末尾に置く
        textEdit->moveCursor(QTextCursor::End);
    }
}

void MultiLineEditDelegate::setModelData(QWidget* _editor, QAbstractItemModel* model, 
                                       const QModelIndex& index) const
{
    auto* editor = qobject_cast<EditorWidgetWrapper*>(_editor);
    if (editor) {
        auto textEdit = editor->getTextEdit();
        QString text = textEdit->toPlainText();
        if(auto* csvEditor = getCSVEditor(this)) {
            csvEditor->setText(index, text);
        }
    }
}

void MultiLineEditDelegate::updateEditorGeometry(QWidget* editorWidget,
                                               const QStyleOptionViewItem& viewItemOption,
                                               const QModelIndex& index) const
{
    auto* textEdit = editorWidget->findChild<MultiLineTextEdit*>();
    if(textEdit != nullptr) {
        // セルの座標とサイズを取得
        QRect geometryRect = viewItemOption.rect;

        // テキストの内容に基づいて必要な高さを計算
        QString text = index.model()->data(index, Qt::EditRole).toString();
        QFontMetrics fontMetrics(textEdit->font());

        // 改行数に基づいたテキスト領域の高さ + ガイドラベルの高さ(18px)
        int lineCount = text.split('\n').size();
        int textHeight = fontMetrics.lineSpacing() * qMax(3, lineCount) + 10;
        int totalHeight = textHeight + 18;

        // セルの幅はそのまま使い、高さは「セルの高さ」と「必要高さ」の大きい方にする
        geometryRect.setHeight(qMax(geometryRect.height(), totalHeight));

        // ビューポート内での位置を正しく設定
        editorWidget->setGeometry(geometryRect);
    }
}

QSize MultiLineEditDelegate::sizeHint(const QStyleOptionViewItem& option, 
                                    const QModelIndex& index) const
{
    QSize size = QStyledItemDelegate::sizeHint(option, index);
    
    auto model = index.model();
    if(model == nullptr) { return size; }

    // テキストが改行を含む場合、高さを調整
    QString text = model->data(index, Qt::DisplayRole).toString();
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

void MultiLineEditDelegate::MultiLineTextEdit::finishEditing(MoveTo moveTo)
{
    if(_finished) { return; }
    _finished = true;
    emit editingFinished(moveTo);
}

bool MultiLineEditDelegate::MultiLineTextEdit::event(QEvent* event)
{
    //Tab は QWidget::event がフォーカス移動に使い keyPressEvent まで届かないため、ここで受け取る
    if(event->type() == QEvent::KeyPress)
    {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if(keyEvent->key() == Qt::Key_Tab && keyEvent->modifiers() == Qt::NoModifier)
        {
            finishEditing(MoveTo::Right);
            return true;
        }
    }
    return QTextEdit::event(event);
}

void MultiLineEditDelegate::MultiLineTextEdit::focusOutEvent(QFocusEvent* event)
{
    QTextEdit::focusOutEvent(event);

    //デリゲートのイベントフィルタはラッパーに付いており、フォーカスプロキシのこのウィジェットの FocusOut を受け取れない。
    //そのため、表の外（保存ボタンなど）をクリックした場合もここで入力内容を確定する。
    if(event->reason() == Qt::PopupFocusReason) { return; } //右クリックメニューなど
    for(auto* w = QApplication::focusWidget(); w != nullptr; w = w->parentWidget()) {
        if(w == this->parentWidget()) { return; } //エディタ内でのフォーカス移動
    }
    finishEditing(MoveTo::None);
}

void MultiLineEditDelegate::MultiLineTextEdit::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) 
    {
        // Ctrl+Enter または Shift+Enter で編集完了
        if (event->modifiers() & Qt::ControlModifier) {
            finishEditing(MoveTo::None);
            return;
        }
        else if(event->modifiers() & Qt::ShiftModifier) {
            finishEditing(MoveTo::Down);
            return;
        }
        // 通常のEnterは改行
        QTextEdit::keyPressEvent(event);
    } else if(event->key() == Qt::Key_Escape) {
        //閉じる際の FocusOut で確定しないよう、終了済みにしてから通知する
        _finished = true;
        emit editingCanceled();
    } else {
        QTextEdit::keyPressEvent(event);
    }
}

#include "MultiLineEditDelegate.moc"