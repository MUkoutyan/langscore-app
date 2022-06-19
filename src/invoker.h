#pragma once

#include "src/ui/ComponentBase.h"
#include <QObject>
#include <QProcess>

class invoker : public QObject, public ComponentBase
{
    Q_OBJECT
public:
    enum ProjectType {
        VXAce,
        VX,
        VZ
    };

    invoker(ComponentBase::Common::Type setting);

    bool run();

signals:
    void getStdOut(QString);

private:
    QProcess* process;
};

