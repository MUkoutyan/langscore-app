#pragma once

#include "PopupDialogBase.h"
#include "../settings.h"

namespace Ui {
class FirstWriteDialog;
}

class FirstWriteDialog : public PopupDialogBase
{
    Q_OBJECT

public:
    explicit FirstWriteDialog(std::shared_ptr<settings> setting, QWidget *parent = nullptr);
    ~FirstWriteDialog();

    void setOutputPath(QString directoryPath);
    QString outputPath() const ;

    bool writeByLanguagePatch() const;
    int writeMode() const;
    bool backup() const;

private:
    Ui::FirstWriteDialog *ui;
    int _writeMode;

private slots:
    void changeOutputPath(const QString& path);

#if defined(LANGSCORE_GUIAPP_TEST)
    friend class LangscoreAppTest;
#endif
};

