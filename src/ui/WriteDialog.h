#pragma once

#include "PopupDialogBase.h"
#include "../settings.h"

namespace Ui {
class WriteDialog;
}

class WriteDialog : public PopupDialogBase
{
    Q_OBJECT

public:
    explicit WriteDialog(std::shared_ptr<settings> setting, QWidget *parent = nullptr);
    ~WriteDialog();

    void setOutputPath(QString directoryPath);
    QString outputPath() const ;

    bool writeByLanguage() const;
    int writeMode() const;
    bool backup() const;

private:
    Ui::WriteDialog *ui;
    int _writeMode;

private slots:
    void changeOutputPath(const QString& path);

#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

