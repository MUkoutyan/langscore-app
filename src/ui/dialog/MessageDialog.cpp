#include "MessageDialog.h"
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QPushButton>


MessageDialog::MessageDialog(QWidget *parent)
    : PopupDialogBase(parent)
    , messageLabel(new QLabel(this))
    , buttonBox(new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this))
{
    QVBoxLayout* base = new QVBoxLayout(this);
    this->setLayout(base);

    base->addWidget(this->messageLabel);

    base->addSpacing(12);
    base->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void MessageDialog::setLabelText(QString text)
{
    this->messageLabel->setText(text);
}

void MessageDialog::okButtonText(QString text)
{
    auto button = buttonBox->button(QDialogButtonBox::Ok);
    button->setText(text);
}

void MessageDialog::cancelButtonText(QString text)
{
    auto button = buttonBox->button(QDialogButtonBox::Cancel);
    button->setText(text);
}
