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
    explicit MultiLineTextEdit(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    // 編集終了を1度だけ通知する（確定でエディタを閉じる際の FocusOut で二重に通知しないため）
    void finishEditing(bool shouldMoveDown);
    bool _finished = false;

signals:
    // 編集終了のヒント（次のセルへ行くか等）を渡せるように変更
    void editingFinished(bool shouldMoveDown);
};