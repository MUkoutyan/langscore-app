#include "TranslationApiSettingsDialog.h"
#include "ui_TranslationApiSettingsDialog.h"
#include <QMessageBox>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
#include <QSettings>
#include <QApplication>

TranslationApiSettingsDialog::TranslationApiSettingsDialog(QWidget *parent)
    : PopupDialogBase(parent)
    , ui(new Ui::TranslationApiSettingsDialog)
    , _networkManager(new QNetworkAccessManager(this))
{
    ui->setupUi(this);
    setWindowTitle(tr("Translation API Settings"));
    setModal(true);
    
    setupConnections();
    loadSettings();
    updateUsageDisplay();
}

TranslationApiSettingsDialog::~TranslationApiSettingsDialog()
{
    delete ui;
}

void TranslationApiSettingsDialog::setupConnections()
{
    // API Key input connections
    connect(ui->deeplApiKeyEdit, &QLineEdit::textChanged, 
            this, &TranslationApiSettingsDialog::onDeepLApiKeyChanged);
    connect(ui->googleApiKeyEdit, &QLineEdit::textChanged, 
            this, &TranslationApiSettingsDialog::onGoogleApiKeyChanged);
    
    // Test connection buttons
    connect(ui->testDeepLButton, &QPushButton::clicked, 
            this, &TranslationApiSettingsDialog::onTestDeepLConnection);
    connect(ui->testGoogleButton, &QPushButton::clicked, 
            this, &TranslationApiSettingsDialog::onTestGoogleConnection);
    
    // Show/Hide API key buttons
    connect(ui->showDeepLKeyButton, &QPushButton::toggled, 
            this, [this](bool checked) {
                ui->deeplApiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
                ui->showDeepLKeyButton->setText(checked ? tr("Hide") : tr("Show"));
            });
    connect(ui->showGoogleKeyButton, &QPushButton::toggled, 
            this, [this](bool checked) {
                ui->googleApiKeyEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
                ui->showGoogleKeyButton->setText(checked ? tr("Hide") : tr("Show"));
            });
    
    // Refresh usage button
    connect(ui->refreshUsageButton, &QPushButton::clicked, 
            this, &TranslationApiSettingsDialog::refreshUsageInfo);
    
    // Dialog buttons
    connect(ui->buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void TranslationApiSettingsDialog::loadSettings()
{
    ui->deeplApiKeyEdit->setText(getDeepLApiKey());
    ui->googleApiKeyEdit->setText(getGoogleApiKey());
    ui->deeplApiKeyEdit->setEchoMode(QLineEdit::Password);
    ui->googleApiKeyEdit->setEchoMode(QLineEdit::Password);
    
    validateApiKeys();
}

void TranslationApiSettingsDialog::saveSettings()
{
    setDeepLApiKey(ui->deeplApiKeyEdit->text().trimmed());
    setGoogleApiKey(ui->googleApiKeyEdit->text().trimmed());
}

QString TranslationApiSettingsDialog::getDeepLApiKey() const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    return settings.value("translation/deeplApiKey", "").toString();
}

QString TranslationApiSettingsDialog::getGoogleApiKey() const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    return settings.value("translation/googleApiKey", "").toString();
}

void TranslationApiSettingsDialog::setDeepLApiKey(const QString& key)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.setValue("translation/deeplApiKey", key);
}

void TranslationApiSettingsDialog::setGoogleApiKey(const QString& key)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.setValue("translation/googleApiKey", key);
}

qint64 TranslationApiSettingsDialog::getDeepLUsage() const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    return settings.value("translation/deeplUsage", 0).toLongLong();
}

qint64 TranslationApiSettingsDialog::getGoogleUsage() const
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    return settings.value("translation/googleUsage", 0).toLongLong();
}

void TranslationApiSettingsDialog::setDeepLUsage(qint64 usage)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.setValue("translation/deeplUsage", usage);
}

void TranslationApiSettingsDialog::setGoogleUsage(qint64 usage)
{
    QSettings settings(qApp->applicationDirPath() + "/settings.ini", QSettings::IniFormat);
    settings.setValue("translation/googleUsage", usage);
}

void TranslationApiSettingsDialog::onDeepLApiKeyChanged()
{
    validateApiKeys();
}

void TranslationApiSettingsDialog::onGoogleApiKeyChanged()
{
    validateApiKeys();
}

void TranslationApiSettingsDialog::validateApiKeys()
{
    QString deeplKey = ui->deeplApiKeyEdit->text().trimmed();
    QString googleKey = ui->googleApiKeyEdit->text().trimmed();
    
    ui->testDeepLButton->setEnabled(!deeplKey.isEmpty());
    ui->testGoogleButton->setEnabled(!googleKey.isEmpty());
    
    // Update status labels
    ui->deeplStatusLabel->setText(deeplKey.isEmpty() ? 
        tr("API key not set") : tr("API key configured"));
    ui->googleStatusLabel->setText(googleKey.isEmpty() ? 
        tr("API key not set") : tr("API key configured"));
}

void TranslationApiSettingsDialog::onTestDeepLConnection()
{
    QString apiKey = ui->deeplApiKeyEdit->text().trimmed();
    if (apiKey.isEmpty()) return;
    
    ui->testDeepLButton->setEnabled(false);
    ui->deeplStatusLabel->setText(tr("Testing connection..."));
    
    testApiConnection("deepl", apiKey);
}

void TranslationApiSettingsDialog::onTestGoogleConnection()
{
    QString apiKey = ui->googleApiKeyEdit->text().trimmed();
    if (apiKey.isEmpty()) return;
    
    ui->testGoogleButton->setEnabled(false);
    ui->googleStatusLabel->setText(tr("Testing connection..."));
    
    testApiConnection("google", apiKey);
}

void TranslationApiSettingsDialog::testApiConnection(const QString& service, const QString& apiKey)
{
    QNetworkRequest request;
    QNetworkReply* reply = nullptr;
    
    if (service == "deepl") {
        // Test DeepL API with usage endpoint
        QUrl url("https://api-free.deepl.com/v2/usage");
        request.setUrl(url);
        request.setRawHeader("Authorization", QString("DeepL-Auth-Key %1").arg(apiKey).toUtf8());
        reply = _networkManager->get(request);
        connect(reply, &QNetworkReply::finished, 
                this, [this, reply]() { onDeepLTestFinished(reply); });
    }
    else if (service == "google") {
        // Test Google Translate API with languages endpoint
        QUrl url("https://translation.googleapis.com/language/translate/v2/languages");
        QUrlQuery query;
        query.addQueryItem("key", apiKey);
        url.setQuery(query);
        request.setUrl(url);
        reply = _networkManager->get(request);
        connect(reply, &QNetworkReply::finished, 
                this, [this, reply]() { onGoogleTestFinished(reply); });
    }
    
    if (reply) {
        // Set timeout
        QTimer::singleShot(10000, reply, [reply]() {
            if (reply->isRunning()) {
                reply->abort();
            }
        });
    }
}

void TranslationApiSettingsDialog::onDeepLTestFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    ui->testDeepLButton->setEnabled(true);
    
    if (reply->error() == QNetworkReply::NoError) {
        ui->deeplStatusLabel->setText(tr("Connection successful"));
        ui->deeplStatusLabel->setStyleSheet("color: green;");
        
        // Parse usage info if available
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("character_count") && obj.contains("character_limit")) {
                int used = obj["character_count"].toInt();
                int limit = obj["character_limit"].toInt();
                ui->deeplUsageLabel->setText(tr("Usage: %1 / %2 characters").arg(used).arg(limit));
                setDeepLUsage(used);
            }
        }
    } else {
        ui->deeplStatusLabel->setText(tr("Connection failed: %1").arg(reply->errorString()));
        ui->deeplStatusLabel->setStyleSheet("color: red;");
    }
}

void TranslationApiSettingsDialog::onGoogleTestFinished(QNetworkReply* reply)
{
    reply->deleteLater();
    ui->testGoogleButton->setEnabled(true);
    
    if (reply->error() == QNetworkReply::NoError) {
        ui->googleStatusLabel->setText(tr("Connection successful"));
        ui->googleStatusLabel->setStyleSheet("color: green;");
    } else {
        ui->googleStatusLabel->setText(tr("Connection failed: %1").arg(reply->errorString()));
        ui->googleStatusLabel->setStyleSheet("color: red;");
    }
}

void TranslationApiSettingsDialog::refreshUsageInfo()
{
    ui->refreshUsageButton->setEnabled(false);
    
    // Test both APIs to get fresh usage info
    QString deeplKey = ui->deeplApiKeyEdit->text().trimmed();
    if (!deeplKey.isEmpty()) {
        testApiConnection("deepl", deeplKey);
    }
    
    QTimer::singleShot(2000, this, [this]() {
        ui->refreshUsageButton->setEnabled(true);
    });
}

void TranslationApiSettingsDialog::updateUsageDisplay()
{
    qint64 deeplUsage = getDeepLUsage();
    qint64 googleUsage = getGoogleUsage();
    
    ui->deeplUsageLabel->setText(tr("Characters used: %1").arg(deeplUsage));
    ui->googleUsageLabel->setText(tr("Characters used: %1").arg(googleUsage));
}