#pragma once

#include "settings.h"
#include <QObject>
#include <QProcess>

class invoker : public QObject
{
    Q_OBJECT
public:
    enum ProjectType {
        VXAce,
        VX,
        VZ
    };

    invoker(settings setting);

    bool run();

signals:
    void getStdOut(QString);

private:
    settings setting;
    QProcess* process;
};

