#include "PopupDialogBase.h"

PopupDialogBase::PopupDialogBase(QWidget *parent)
    : QDialog(parent)
{
    this->setWindowFlag(Qt::FramelessWindowHint);
    this->setStyleSheet("#writeDialog{border: 2px solid #ececec;}");
}

PopupDialogBase::~PopupDialogBase(){
}
