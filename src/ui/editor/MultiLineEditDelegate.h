#pragma once

#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QKeyEvent>

class MultiLineEditDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    class MultiLineTextEdit;

    explicit MultiLineEditDelegate(QObject* parent = nullptr);

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option, 
                         const QModelIndex& index) const override;
    void setEditorData(QWidget* editor, const QModelIndex& index) const override;
    void setModelData(QWidget* editor, QAbstractItemModel* model, 
                     const QModelIndex& index) const override;
    void updateEditorGeometry(QWidget* editor, const QStyleOptionViewItem& option, 
                             const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;

private slots:
    void commitAndCloseEditor();

};

class MultiLineEditDelegate::MultiLineTextEdit : public QTextEdit {
    Q_OBJECT

public:
    //編集を確定した後に移るセル
    enum class MoveTo { None, Down, Right };

    explicit MultiLineTextEdit(QWidget* parent = nullptr);

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    // 編集終了を1度だけ通知する（確定でエディタを閉じる際の FocusOut で二重に通知しないため）
    void finishEditing(MoveTo moveTo);
    bool _finished = false;

signals:
    // 編集終了と、次に移るセルを通知する
    void editingFinished(MultiLineEditDelegate::MultiLineTextEdit::MoveTo moveTo);
    // 入力を取り消して閉じる（Esc）
    void editingCanceled();
};