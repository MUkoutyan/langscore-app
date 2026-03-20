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
        auto* guideLabel = new QLabel("Ctrl+Enter:Confirm. Tab or Shift+Enter:Confirm and move next.", this);
        guideLabel->setStyleSheet("color: white; font-size: 8pt; background-color: #505050;");
        guideLabel->setAlignment(Qt::AlignRight);
        // ラベルがエディタを圧迫しないよう高さを固定
        guideLabel->setFixedHeight(18);

        containerLayout->addWidget(textEdit);
        containerLayout->addWidget(guideLabel);

        this->setFocusProxy(textEdit);


        connect(textEdit, &MultiLineEditDelegate::MultiLineTextEdit::editingFinished, [delegate, this](bool shouldMoveDown) {
            emit delegate->commitData(this);
            emit delegate->closeEditor(this);

            if(shouldMoveDown == true) 
            {
                if(auto editor = getCSVEditor(this)) {
                    editor->selectAndEditNextCell();
                }
            }
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
        textEdit->setPlainText(text);
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
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) 
    {
        // Ctrl+Enter または Shift+Enter で編集完了
        if (event->modifiers() & Qt::ControlModifier) {
            emit editingFinished(false);
            return;
        }
        else if(event->modifiers() & Qt::ShiftModifier) {
            emit editingFinished(true);
        }
        // 通常のEnterは改行
        QTextEdit::keyPressEvent(event);
    } else if (event->key() == Qt::Key_Tab) {
        // Tabで編集完了
        emit editingFinished(true);
    } else {
        QTextEdit::keyPressEvent(event);
    }
}

#include "MultiLineEditDelegate.moc"