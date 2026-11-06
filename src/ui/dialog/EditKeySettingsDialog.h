#pragma once
#include <QDialog>
#include <array>
#include "../editor/MultiLineEditDelegate.h"

class QKeySequenceEdit;

//セルの編集で使うキー（確定・改行など）を変えるダイアログ
class EditKeySettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit EditKeySettingsDialog(QWidget* parent = nullptr);

    EditKeyBindings bindings() const;
    void setBindings(const EditKeyBindings& bindings);

    //同じキーを複数の動作に割り当てていれば、その動作の名前を返す
    static QStringList duplicatedActions(const EditKeyBindings& bindings);

private:
    std::array<QKeySequenceEdit*, EditKeyBindings::Count> keyEdits;
};
