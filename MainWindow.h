#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "src/settings.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class FormTaskBar;
class MainComponent;
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    std::shared_ptr<settings> setting;
    FormTaskBar* taskBar;
    MainComponent* mainComponent;

private slots:
    void changeMaximumState();

    QString openOutputProjectDir(QString root);

    void openGameProject(QString path);
};
#endif // MAINWINDOW_H
