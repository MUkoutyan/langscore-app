#pragma once

#include <QApplication>
#include <QDialog>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QColorDialog>
#include <QPalette>
#include <vector>


class ColorDialog : public QDialog
{
    Q_OBJECT

public:
    ColorDialog(QWidget *parent = nullptr);

private:
    QList<QPushButton *> createButtons(QPalette::ColorGroup colorGroup);
    void createLayout();
private:
    QGridLayout *m_layout;
    QLabel *m_colorGroupLabels[QPalette::NColorGroups];
    QList<QList<QPushButton*>> m_colorGroupButtons;
};
