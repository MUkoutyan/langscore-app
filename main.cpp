#include "MainWindow.h"
#include "application_config.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QStyleFactory>
#include <QDir>
#include <QFileInfoList>
#include <QMessageBox>

bool checkInstallValid(const QApplication& a)
{
    QStringList files = {
        "/resources/fonts/mplus-1m-regular.ttf",
        "/resources/fonts/VL-Gothic-Regular.ttf",
        "/bin/divisi.exe",
        "/bin/rvcnv.exe",
        "/bin/resource/langscore.rb",
        "/bin/resource/lscsv.rb",
        "/bin/resource/vocab.csv"
    };
    for(const auto& file : files){
        if(QFile(a.applicationDirPath()+file).exists() == false){
            QMessageBox::critical(nullptr, "Langscore Error!", QObject::tr("The required file does not exist. Reinstall the application."));
            return false;
        }
    }

    return true;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyle(QStyleFactory::create("Fusion"));
    a.setApplicationVersion(PROJECT_VER);

    if(checkInstallValid(a) == false){
        return -2;
    }

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "langscore-app_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;
    w.show();
    return a.exec();
}
