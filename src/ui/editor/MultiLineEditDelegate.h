#pragma once

#include <QStyledItemDelegate>
#include <QTextEdit>
#include <QKeyEvent>
#include <QKeySequence>
#include <array>

//セルの編集で使うキー。アプリの設定ファイル settings.ini に保存する
struct EditKeyBindings
{
    enum Action { Confirm, ConfirmAndMoveDown, ConfirmAndMoveRight, NewLine, Cancel, Count };
    std::array<QKeySequence, Count> keys;

    static EditKeyBindings defaults();
    static EditKeyBindings load();
    void save() const;
    static QString actionName(Action action);
    //押したキーに割り当てた動作。無ければ Count
    Action find(const QKeyEvent* event) const;
    //テンキーの Enter と Enter、Shift+Tab の表現の違いを統一したキー。同じキーかの比較に使う
    static QKeySequence normalized(const QKeySequence& key);
    //エディタの下に表示する、動作ごとのキーの説明
    QStringList guideItems() const;
};

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
};

class MultiLineEditDelegate::MultiLineTextEdit : public QTextEdit {
    Q_OBJECT

public:
    //編集を確定した後に移るセル
    enum class MoveTo { None, Down, Right };

    explicit MultiLineTextEdit(QWidget* parent = nullptr);
    void setKeyBindings(const EditKeyBindings& bindings) { _keys = bindings; }

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private:
    EditKeyBindings _keys = EditKeyBindings::defaults();
    //割り当てたキーなら、その動作をして true を返す
    bool handleKey(QKeyEvent* event);

    // 編集終了を1度だけ通知する。確定でエディタを閉じる際の FocusOut による二重通知を防ぐ
    void finishEditing(MoveTo moveTo);
    bool _finished = false;

signals:
    // 編集終了と、次に移るセルを通知する
    void editingFinished(MultiLineEditDelegate::MultiLineTextEdit::MoveTo moveTo);
    // Esc で入力を取り消して閉じる
    void editingCanceled();
};