#pragma once

#include <QDialog>
#include "../settings.h"

namespace Ui {
class WriteDialog;
}

class WriteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WriteDialog(std::shared_ptr<settings> setting, QWidget *parent = nullptr);
    ~WriteDialog();

    void setOutputPath(QString directoryPath);
    QString outputPath() const ;

    bool writeByLanguage() const;

private:
    Ui::WriteDialog *ui;
};

