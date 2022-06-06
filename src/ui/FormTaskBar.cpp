#include "FormTaskBar.h"
#include "ui_FormTaskBar.h"

#include <QMenuBar>
#include <QMouseEvent>

FormTaskBar::FormTaskBar(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::FormTaskBar)
    , pressLeftButton(false)
{
    ui->setupUi(this);

    connect(this->ui->closeButton,   &QPushButton::clicked, this, &FormTaskBar::pushClose);
    connect(this->ui->maximumButton, &QPushButton::clicked, this, &FormTaskBar::maximum);
    connect(this->ui->minimumButton, &QPushButton::clicked, this, &FormTaskBar::minimum);

    auto menuBar = new QMenuBar(this);
    this->ui->horizontalLayout_2->insertWidget(0, menuBar);
    auto fileMenu = menuBar->addMenu(tr("File"));
    fileMenu->addAction(tr("Open Game Project"));
    fileMenu->addAction(tr("Save Langscore Project"));
    fileMenu->addAction(tr("Save as Langscore Project"));
    fileMenu->addAction(tr("Quit"));
    auto helpMenu = menuBar->addMenu(tr("Help"));

    this->ui->horizontalLayout_2->setStretch(2, 1);
}

FormTaskBar::~FormTaskBar()
{
    delete ui;
}

void FormTaskBar::mousePressEvent(QMouseEvent *event)
{
    QWidget::mousePressEvent(event);
    pressLeftButton = event->button() & Qt::LeftButton;
    this->beforeMousePos = event->pos();
}

void FormTaskBar::mouseMoveEvent(QMouseEvent *event)
{
    QWidget::mouseMoveEvent(event);
    if(pressLeftButton){
        emit this->dragging(event, event->pos() - this->beforeMousePos);
    }
}

void FormTaskBar::mouseReleaseEvent(QMouseEvent *event)
{
    QWidget::mouseReleaseEvent(event);
    pressLeftButton = false;
    beforeMousePos = QPoint();
}

void FormTaskBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
    emit this->doubleClick();
}

