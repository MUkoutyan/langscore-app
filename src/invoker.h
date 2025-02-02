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
    void reanalysis(bool sync = false);
    void exportFirstTime(bool sync = false);
    void exportCSV(bool sync = false);
    void updatePlugin(bool sync = false);
    void validate(bool sync = false);
    void packing(bool sync = false);

    QStringList lastProcessOption() const noexcept { return passOption; }

signals:
    void getStdOut(QString);
    void update();
    void finish(int exitCode, QProcess::ExitStatus status);

private:
    void doProcess(QStringList option);
    bool sync;
    QStringList passOption;
};

