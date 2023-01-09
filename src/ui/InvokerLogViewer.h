#pragma once

#include <QPlainTextEdit>
#include "../invoker.h"
#include "ComponentBase.h"

class InvokerLogViewer : public QPlainTextEdit, public ComponentBase
{
    Q_OBJECT
public:
    InvokerLogViewer(QWidget* parent);
    void SetSettings(std::shared_ptr<settings> setting);

public slots:
    void writeText(QString text);

private:
    std::shared_ptr<settings> setting;
};

