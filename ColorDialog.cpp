#include "ColorDialog.h"
#include <QMetaEnum>
#include <QStyle>

ColorDialog::ColorDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Color Dialog"));

    for (int i = 0; i < QPalette::NColorGroups; ++i) {
        m_colorGroupButtons.append(createButtons(static_cast<QPalette::ColorGroup>(i)));
    }

    createLayout();
}

QList<QPushButton *> ColorDialog::createButtons(QPalette::ColorGroup colorGroup)
{
    QList<QPushButton *> buttonList;

    QMetaEnum paletteEnum = QMetaEnum::fromType<QPalette::ColorRole>();

    for (int i = 0; i < QPalette::NColorRoles; ++i) {
        QPalette pal = qApp->palette();
        QColor bgColor = pal.color(static_cast<QPalette::ColorRole>(i));

        QPushButton *button = new QPushButton(paletteEnum.valueToKey(i));
        button->setFixedSize(100, 20);
        button->setStyleSheet(QString("background-color: %1; color: %2").arg(bgColor.name(), bgColor.value() < 128 ? "#ffffff" : "#000000"));
        connect(button, &QPushButton::clicked, this, [=] {
            QColor color = QColorDialog::getColor(qApp->palette().color(static_cast<QPalette::ColorRole>(i)), this);
            if (color.isValid()) {
                QPalette pal = qApp->palette();
                pal.setColor(colorGroup, static_cast<QPalette::ColorRole>(i), color);
                qApp->setPalette(pal);
                button->setStyleSheet(QString("background-color: %1; color: %2").arg(color.name(), color.value() < 128 ? "#ffffff" : "#000000"));
                qApp->style()->polish(pal);
            }
        });
        buttonList.append(button);
    }

    return buttonList;
}

void ColorDialog::createLayout()
{
    m_layout = new QGridLayout(this);

    for (int i = 0; i < QPalette::NColorGroups; ++i) {
        m_layout->addWidget(new QLabel(QMetaEnum::fromType<QPalette::ColorGroup>().valueToKey(i)), 0, i * 2 + 1);

        for (int j = 0; j < QPalette::NColorRoles; ++j) {
            m_layout->addWidget(m_colorGroupButtons[i][j], j + 1, i * 2 + 2);
        }
    }

    for (int i = 0; i < QPalette::NColorRoles; ++i) {
        QLabel *label = new QLabel(qApp->palette().color(static_cast<QPalette::ColorRole>(i)).name());
        m_layout->addWidget(label, i + 1, 0);
    }

    m_layout->setColumnStretch(0, 1);
    for (int i = 0; i < QPalette::NColorGroups; ++i) {
        m_layout->setColumnStretch(i * 2 + 1, 1);
        m_layout->setColumnStretch(i * 2 + 2, 3);
    }

    this->setLayout(m_layout);
}
