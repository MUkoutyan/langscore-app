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

    void analyze(bool sync = false);
    void updateData(bool sync = false);
    void write(bool sync = false);
    void validate(bool sync = false);
    void packing(bool sync = false);

signals:
    void getStdOut(QString);
    void update();
    void finish(int exitCode);

private:
    void doProcess(QStringList option);
    bool sync;
};

