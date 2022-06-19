#include "SwitchButton.h"

SwitchButton::SwitchButton(QWidget* parent)
    : QPushButton(parent)
{
    this->setCheckable(true);
    this->setIcon(QIcon(":/flags/us.svg"));
}
