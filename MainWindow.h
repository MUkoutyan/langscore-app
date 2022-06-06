#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

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
    FormTaskBar* taskBar;
    MainComponent* mainComponent;

private slots:
    void changeMaximumState();

    QString openOutputProjectDir(QString root);
};
#endif // MAINWINDOW_H
