#pragma once

#include "PopupDialogBase.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>

namespace Ui {
class TranslationApiSettingsDialog;
}

class TranslationApiSettingsDialog : public PopupDialogBase
{
    Q_OBJECT

public:
    explicit TranslationApiSettingsDialog(QWidget *parent = nullptr);
    ~TranslationApiSettingsDialog();

    void loadSettings();
    void saveSettings();

private slots:
    void onDeepLApiKeyChanged();
    void onGoogleApiKeyChanged();
    void onTestDeepLConnection();
    void onTestGoogleConnection();
    void onDeepLTestFinished(QNetworkReply* reply);
    void onGoogleTestFinished(QNetworkReply* reply);
    void refreshUsageInfo();

private:
    Ui::TranslationApiSettingsDialog *ui;
    QNetworkAccessManager* _networkManager;
    
    void setupConnections();
    void validateApiKeys();
    void testApiConnection(const QString& service, const QString& apiKey);
    void updateUsageDisplay();
    
    // Settings.ini access methods
    QString getDeepLApiKey() const;
    QString getGoogleApiKey() const;
    void setDeepLApiKey(const QString& key);
    void setGoogleApiKey(const QString& key);
    qint64 getDeepLUsage() const;
    qint64 getGoogleUsage() const;
    void setDeepLUsage(qint64 usage);
    void setGoogleUsage(qint64 usage);
};