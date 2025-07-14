#include "TranslationProgressDialog.h"
#include "ui_TranslationProgressDialog.h"
#include <QTimer>

TranslationProgressDialog::TranslationProgressDialog(QWidget *parent)
    : PopupDialogBase(parent)
    , ui(new Ui::TranslationProgressDialog)
    , updateTimer(new QTimer(this))
    , isCancelled(false)
{
    ui->setupUi(this);
    setWindowTitle(tr("Translation Progress"));
    setModal(true);
    
    connect(ui->cancelButton, &QPushButton::clicked, this, &TranslationProgressDialog::onCancelClicked);
    
    updateTimer->setSingleShot(true);
    updateTimer->setInterval(100);
}

TranslationProgressDialog::~TranslationProgressDialog()
{
    delete ui;
}

void TranslationProgressDialog::startProgress(int totalRequests)
{
    isCancelled = false;
    ui->progressBar->setRange(0, totalRequests);
    ui->progressBar->setValue(0);
    ui->statusLabel->setText(tr("Starting translation..."));
    ui->currentTextLabel->clear();
    ui->cancelButton->setEnabled(true);
    ui->cancelButton->setText(tr("Cancel"));
}

void TranslationProgressDialog::updateProgress(int completed, int total)
{
    if (isCancelled) return;
    
    ui->progressBar->setValue(completed);
    ui->statusLabel->setText(tr("Translating... %1 of %2 completed").arg(completed).arg(total));
    
    // Auto-close if completed
    if (completed >= total) {
        QTimer::singleShot(1000, this, [this]() {
            if (!isCancelled) {
                accept();
            }
        });
    }
}

void TranslationProgressDialog::setCurrentText(const QString& text)
{
    QString displayText = text;
    if (displayText.length() > 100) {
        displayText = displayText.left(97) + "...";
    }
    ui->currentTextLabel->setText(tr("Translating: %1").arg(displayText));
}

void TranslationProgressDialog::showError(const QString& errorMessage)
{
    ui->statusLabel->setText(tr("Translation error: %1").arg(errorMessage));
    ui->statusLabel->setStyleSheet("color: red;");
    ui->cancelButton->setText(tr("Close"));
}

void TranslationProgressDialog::showCompletion(int successCount, int totalCount)
{
    if (successCount == totalCount) {
        ui->statusLabel->setText(tr("Translation completed successfully! %1 texts translated.").arg(successCount));
        ui->statusLabel->setStyleSheet("color: green;");
    } else {
        ui->statusLabel->setText(tr("Translation completed with errors. %1 of %2 texts translated successfully.").arg(successCount).arg(totalCount));
        ui->statusLabel->setStyleSheet("color: orange;");
    }
    ui->cancelButton->setText(tr("Close"));
    ui->currentTextLabel->clear();
}

void TranslationProgressDialog::onCancelClicked()
{
    if (ui->cancelButton->text() == tr("Cancel")) {
        isCancelled = true;
        ui->cancelButton->setEnabled(false);
        ui->statusLabel->setText(tr("Cancelling translation..."));
        emit cancelled();
    } else {
        reject();
    }
}