#include "MainWindow.h"
#include "./ui_MainWindow.h"
#include "./src/ui/FormTaskBar.h"
#include "./src/ui/MainComponent.h"

#include <QMouseEvent>
#include <QSettings>
#include <QSizeGrip>
#include <QGraphicsDropShadowEffect>
#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , taskBar(new FormTaskBar(this))
    , mainComponent(new MainComponent(this))
{
    ui->setupUi(this);
    this->setWindowFlag(Qt::FramelessWindowHint, true);

//    QGraphicsDropShadowEffect* effect = new QGraphicsDropShadowEffect();
//    effect->setBlurRadius(5);
//    this->centralWidget()->setGraphicsEffect(effect);

    this->ui->verticalLayout->insertWidget(0, taskBar);
    this->ui->verticalLayout->insertWidget(1, mainComponent);
    this->ui->verticalLayout->addWidget(new QSizeGrip(this), 0, Qt::AlignBottom | Qt::AlignRight);
    this->ui->verticalLayout->setStretch(1,1);

    connect(this->taskBar, &FormTaskBar::pushClose,   this, &QMainWindow::close);
    connect(this->taskBar, &FormTaskBar::maximum,     this, &MainWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::doubleClick, this, &MainWindow::changeMaximumState);
    connect(this->taskBar, &FormTaskBar::minimum,     this, &QMainWindow::showMinimized);

    connect(this->taskBar, &FormTaskBar::dragging, this, [this](QMouseEvent* event, QPoint delta){
        auto pos = this->pos() + delta;
        this->move(pos);
    });

    connect(this->mainComponent, &MainComponent::requestOpenOutputDir, this, &MainWindow::openOutputProjectDir);


#ifdef Q_OS_WIN
    QSettings settings("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",QSettings::NativeFormat);
    if(settings.value("AppsUseLightTheme")==0){
        QPalette darkPalette;
        QColor darkColor = QColor(45,45,45);
        QColor disabledColor = QColor(127,127,127);
        darkPalette.setColor(QPalette::Window, darkColor);
        darkPalette.setColor(QPalette::WindowText, Qt::white);
        darkPalette.setColor(QPalette::Base, QColor(18,18,18));
        darkPalette.setColor(QPalette::AlternateBase, darkColor);
        darkPalette.setColor(QPalette::ToolTipBase, Qt::white);
        darkPalette.setColor(QPalette::ToolTipText, Qt::white);
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::Text, disabledColor);
        darkPalette.setColor(QPalette::Button, darkColor);
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Disabled, QPalette::ButtonText, disabledColor);
        darkPalette.setColor(QPalette::BrightText, Qt::red);
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));

        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::black);
        darkPalette.setColor(QPalette::Disabled, QPalette::HighlightedText, disabledColor);

        qApp->setPalette(darkPalette);

        qApp->setStyleSheet("QToolTip { color: #efefef; background-color: #2a82da; border: 1px solid white; }");
    }
#endif
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::changeMaximumState()
{
    if(this->isMaximized()){
        this->showNormal();
    }
    else {
        this->showMaximized();
    }
}

QString MainWindow::openOutputProjectDir(QString root)
{
    return QFileDialog::getExistingDirectory(this, tr("Open Project File"), root);
}
