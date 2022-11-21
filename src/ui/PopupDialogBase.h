#pragma once

#include <QDialog>

class PopupDialogBase : public QDialog
{
    Q_OBJECT
public:
    PopupDialogBase(QWidget *parent = nullptr);
    ~PopupDialogBase() override;
};

