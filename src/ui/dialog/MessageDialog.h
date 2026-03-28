#pragma once

#include "PopupDialogBase.h"
#include <QLabel>
#include <QDialogButtonBox>

class MessageDialog : public PopupDialogBase
{
    Q_OBJECT
public:
    MessageDialog(QWidget* parent = nullptr);

    void setLabelText(QString text);
    void okButtonText(QString text);
    void cancelButtonText(QString text);

private:
    QLabel* messageLabel;
    QDialogButtonBox* buttonBox;
};

