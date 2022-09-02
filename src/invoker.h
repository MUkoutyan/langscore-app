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

    static constexpr int SUCCESS = 0;

    invoker(ComponentBase* setting);

    int analyze();
    int write();

signals:
    void getStdOut(QString);
    void update();

private:
    int doProcess(QStringList option);
    QProcess* process;
};

