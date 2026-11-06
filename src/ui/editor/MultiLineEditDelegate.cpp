#include "MultiLineEditDelegate.h"
#include "CSVEditor.h"
#include <QApplication>
#include <QFontMetrics>
#include <QVBoxLayout>
#include <QLabel>
#include <QSettings>


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
    EditorWidgetWrapper(MultiLineEditDelegate* delegate, const QModelIndex& index, QWidget* parent)
        :QWidget(parent)
        , editingIndex(index)
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

        const auto keys = EditKeyBindings::load();
        textEdit->setKeyBindings(keys);

        // ガイドラベルの作成。セルの幅に収まるよう、行の分け方は updateGuide で決める
        guideItems = keys.guideItems();
        guideLabel = new QLabel(this);
        guideLabel->setStyleSheet("color: white; background-color: #505050;");
        auto guideFont = guideLabel->font();
        guideFont.setPointSize(8);
        guideLabel->setFont(guideFont);
        guideLabel->setAlignment(Qt::AlignRight);
        updateGuide(this->width());

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
                //今のセルは編集したセルと違う場合があるため、編集したセルから次のセルを決める
                const bool moveDown = (moveTo == MultiLineEditDelegate::MultiLineTextEdit::MoveTo::Down);
                editor->selectAndEditNextCell(editingIndex, moveDown ? 1 : 0, moveDown ? 0 : 1);
            }
        });

        connect(textEdit, &MultiLineEditDelegate::MultiLineTextEdit::editingCanceled, [delegate, this]() {
            emit delegate->closeEditor(this, QAbstractItemDelegate::RevertModelCache);
        });

        //入力して行が増えたら、エディタを大きくする
        connect(textEdit, &QTextEdit::textChanged, this, &EditorWidgetWrapper::fitGeometry);
    }

    MultiLineEditDelegate::MultiLineTextEdit* getTextEdit() const { return textEdit; }

    //セルの位置を受け取り、エディタをそこに置く
    void setCellRect(const QRect& rect)
    {
        cellRect = rect;
        fitGeometry();
    }

    //幅はセルと同じにし、高さは入力中の文章の行数に合わせる。表の下にはみ出す場合は上へずらす
    void fitGeometry()
    {
        if(cellRect.isNull()) { return; }
        QRect rect = cellRect;
        const QFontMetrics fontMetrics(textEdit->font());
        const int lineCount = static_cast<int>(textEdit->toPlainText().count('\n')) + 1;
        const int textHeight = fontMetrics.lineSpacing() * std::max(3, lineCount) + 10;
        rect.setHeight(std::max(rect.height(), textHeight + updateGuide(rect.width())));
        if(auto* area = this->parentWidget(); area != nullptr && area->height() < rect.bottom() + 1) {
            rect.moveBottom(area->height() - 1);
            //表より高い場合は上端に合わせる（はみ出た分はエディタ内でスクロールする）
            if(rect.top() < 0) { rect.moveTop(0); }
        }
        this->setGeometry(rect);
    }

    //キーの説明を、幅に収まるところで改行する（説明の途中では改行しない）。説明の高さを返す
    int updateGuide(int width)
    {
        const QFontMetrics fontMetrics(guideLabel->font());
        const int maxWidth = width - 8;
        QStringList lines;
        QString line;
        for(const auto& item : guideItems)
        {
            const auto joined = line.isEmpty() ? item : line + "  " + item;
            if(line.isEmpty() == false && maxWidth < fontMetrics.horizontalAdvance(joined)) {
                lines.append(line);
                line = item;
            }
            else {
                line = joined;
            }
        }
        if(line.isEmpty() == false) { lines.append(line); }
        guideLabel->setText(lines.join("\n"));
        const int height = fontMetrics.lineSpacing() * static_cast<int>(std::max<qsizetype>(1, lines.size())) + 4;
        // ラベルがエディタを圧迫しないよう高さを固定
        guideLabel->setFixedHeight(height);
        return height;
    }

private:
    QPersistentModelIndex editingIndex;
    QRect cellRect;
    QLabel* guideLabel;
    QStringList guideItems;

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
    return new EditorWidgetWrapper(const_cast<MultiLineEditDelegate*>(this), index, parent);
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
        
        //文字を打って編集を始めた場合は、表計算ソフトと同じく元の文章を置き換える
        if(auto* csvEditor = getCSVEditor(this); csvEditor != nullptr && csvEditor->isReplacingTextOnEdit()) {
            text.clear();
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
    //高さは入力中の文章に合わせて変える（EditorWidgetWrapper::fitGeometry）
    if(auto* wrapper = qobject_cast<EditorWidgetWrapper*>(editorWidget)) {
        wrapper->setCellRect(viewItemOption.rect);
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
    if(event->type() == QEvent::KeyPress && handleKey(static_cast<QKeyEvent*>(event))) {
        return true;
    }
    return QTextEdit::event(event);
}

bool MultiLineEditDelegate::MultiLineTextEdit::handleKey(QKeyEvent* event)
{
    switch(_keys.find(event))
    {
    case EditKeyBindings::Confirm:             finishEditing(MoveTo::None);  return true;
    case EditKeyBindings::ConfirmAndMoveDown:  finishEditing(MoveTo::Down);  return true;
    case EditKeyBindings::ConfirmAndMoveRight: finishEditing(MoveTo::Right); return true;
    case EditKeyBindings::NewLine:             this->insertPlainText("\n"); return true;
    case EditKeyBindings::Cancel:
        //閉じる際の FocusOut で確定しないよう、終了済みにしてから通知する
        _finished = true;
        emit editingCanceled();
        return true;
    default:
        break;
    }
    //改行のキーを別のキーにした場合、割り当てていない Enter では改行しない
    if(event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) { return true; }
    return false;
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
    if(handleKey(event)) { return; }
    QTextEdit::keyPressEvent(event);
}

// EditKeyBindings implementation
namespace
{
    QSettings EditKeySettings() {
        return {qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat};
    }
    const char* EditKeySettingNames[EditKeyBindings::Count] = {
        "EditKeys/confirm", "EditKeys/confirmAndMoveDown", "EditKeys/confirmAndMoveRight", "EditKeys/newLine", "EditKeys/cancel"
    };

    //テンキーの Enter も Enter として扱う
    QKeySequence ToSequence(Qt::Key key, Qt::KeyboardModifiers modifiers)
    {
        if(key == Qt::Key_Enter) { key = Qt::Key_Return; }
        modifiers &= ~Qt::KeypadModifier;
        //Shift+Tab は Backtab として届く
        if(key == Qt::Key_Backtab) { key = Qt::Key_Tab; modifiers |= Qt::ShiftModifier; }
        return QKeySequence(QKeyCombination(modifiers, key));
    }
}

EditKeyBindings EditKeyBindings::defaults()
{
    EditKeyBindings bindings;
    bindings.keys[Confirm]             = QKeySequence(Qt::CTRL | Qt::Key_Return);
    bindings.keys[ConfirmAndMoveDown]  = QKeySequence(Qt::SHIFT | Qt::Key_Return);
    bindings.keys[ConfirmAndMoveRight] = QKeySequence(Qt::Key_Tab);
    bindings.keys[NewLine]             = QKeySequence(Qt::Key_Return);
    bindings.keys[Cancel]              = QKeySequence(Qt::Key_Escape);
    return bindings;
}

EditKeyBindings EditKeyBindings::load()
{
    auto bindings = defaults();
    auto settings = EditKeySettings();
    for(int i = 0; i < Count; ++i) {
        if(settings.contains(EditKeySettingNames[i]) == false) { continue; }
        bindings.keys[i] = QKeySequence::fromString(settings.value(EditKeySettingNames[i]).toString(), QKeySequence::PortableText);
    }
    return bindings;
}

void EditKeyBindings::save() const
{
    auto settings = EditKeySettings();
    for(int i = 0; i < Count; ++i) {
        settings.setValue(EditKeySettingNames[i], keys[i].toString(QKeySequence::PortableText));
    }
}

QString EditKeyBindings::actionName(Action action)
{
    switch(action)
    {
    case Confirm:             return QObject::tr("Confirm");
    case ConfirmAndMoveDown:  return QObject::tr("Confirm and move down");
    case ConfirmAndMoveRight: return QObject::tr("Confirm and move right");
    case NewLine:             return QObject::tr("New line");
    case Cancel:              return QObject::tr("Cancel");
    default:                  return {};
    }
}

EditKeyBindings::Action EditKeyBindings::find(const QKeyEvent* event) const
{
    const auto pressed = ToSequence(static_cast<Qt::Key>(event->key()), event->modifiers());
    for(int i = 0; i < Count; ++i) {
        if(keys[i].isEmpty() == false && normalized(keys[i]) == pressed) {
            return static_cast<Action>(i);
        }
    }
    return Count;
}

QKeySequence EditKeyBindings::normalized(const QKeySequence& key)
{
    if(key.isEmpty()) { return key; }
    return ToSequence(key[0].key(), key[0].keyboardModifiers());
}

QStringList EditKeyBindings::guideItems() const
{
    QStringList texts;
    for(int i = 0; i < Count; ++i) {
        if(keys[i].isEmpty()) { continue; }
        texts.append(keys[i].toString(QKeySequence::NativeText) + ": " + actionName(static_cast<Action>(i)));
    }
    return texts;
}

#include "MultiLineEditDelegate.moc"