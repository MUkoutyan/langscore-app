#include "EditKeySettingsDialog.h"
#include <QKeySequenceEdit>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QMessageBox>
#include <QLabel>

EditKeySettingsDialog::EditKeySettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    this->setWindowTitle(tr("Cell Editing Keys"));

    auto* formLayout = new QFormLayout();
    for(int i = 0; i < EditKeyBindings::Count; ++i)
    {
        auto* keyEdit = new QKeySequenceEdit(this);
        //1つのキーの組み合わせだけを受け付ける
        keyEdit->setMaximumSequenceLength(1);
        keyEdit->setClearButtonEnabled(true);
        keyEdits[i] = keyEdit;
        formLayout->addRow(EditKeyBindings::actionName(static_cast<EditKeyBindings::Action>(i)), keyEdit);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this]() {
        this->setBindings(EditKeyBindings::defaults());
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]()
    {
        const auto keys = this->bindings();
        //取り消しのキーが無いと、入力を取り消す方法が無くなる（表の外をクリックすると確定になる）
        if(keys.keys[EditKeyBindings::Cancel].isEmpty()) {
            QMessageBox::warning(this, tr("Cell Editing Keys"), tr("Please assign a key to \"%1\".").arg(EditKeyBindings::actionName(EditKeyBindings::Cancel)));
            return;
        }
        const auto duplicated = duplicatedActions(keys);
        if(duplicated.isEmpty() == false) {
            QMessageBox::warning(this, tr("Cell Editing Keys"), tr("The same key is assigned to the following actions.") + "\n\n" + duplicated.join("\n"));
            return;
        }
        keys.save();
        this->accept();
    });

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(formLayout);
    layout->addWidget(buttons);

    this->setBindings(EditKeyBindings::load());
}

EditKeyBindings EditKeySettingsDialog::bindings() const
{
    EditKeyBindings keys;
    for(int i = 0; i < EditKeyBindings::Count; ++i) {
        keys.keys[i] = keyEdits[i]->keySequence();
    }
    return keys;
}

void EditKeySettingsDialog::setBindings(const EditKeyBindings& bindings)
{
    for(int i = 0; i < EditKeyBindings::Count; ++i) {
        keyEdits[i]->setKeySequence(bindings.keys[i]);
    }
}

QStringList EditKeySettingsDialog::duplicatedActions(const EditKeyBindings& bindings)
{
    QStringList actions;
    for(int i = 0; i < EditKeyBindings::Count; ++i)
    {
        if(bindings.keys[i].isEmpty()) { continue; }
        for(int j = 0; j < EditKeyBindings::Count; ++j)
        {
            if(i == j || bindings.keys[j].isEmpty()) { continue; }
            if(EditKeyBindings::normalized(bindings.keys[i]) != EditKeyBindings::normalized(bindings.keys[j])) { continue; }
            actions.append(EditKeyBindings::actionName(static_cast<EditKeyBindings::Action>(i)) + " : " + bindings.keys[i].toString(QKeySequence::NativeText));
            break;
        }
    }
    return actions;
}
