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
    //エラー・一部失敗など、前回の結果の色が残らないようにする
    ui->statusLabel->setStyleSheet(QString());
    ui->currentTextLabel->clear();
    ui->messageText->clear();
    ui->messageText->setVisible(false);
    ui->cancelButton->setEnabled(true);
    ui->cancelButton->setText(tr("Cancel"));
}

void TranslationProgressDialog::updateProgress(int completed, int total)
{
    if (isCancelled) return;
    
    ui->progressBar->setValue(completed);
    ui->statusLabel->setText(tr("Translating... %1 of %2 completed").arg(completed).arg(total));
    //閉じるかどうかは、エラー・警告が出たかを見て showCompletion で決める
}

void TranslationProgressDialog::addMessage(const QString& message)
{
    ui->messageText->appendPlainText(message);
    ui->messageText->setVisible(true);
}

bool TranslationProgressDialog::hasMessages() const
{
    return ui->messageText->document()->isEmpty() == false;
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

void TranslationProgressDialog::showCompletion(int successCount, int totalCount, const QString& errorMessage)
{
    if (successCount == totalCount) {
        ui->statusLabel->setText(tr("Translation finished. %1 texts were translated.").arg(successCount));
        ui->statusLabel->setStyleSheet("color: green;");
    } else {
        auto text = tr("Translation finished, but some texts failed. %1 of %2 texts were translated.").arg(successCount).arg(totalCount);
        if(errorMessage.isEmpty() == false) { text += "\n" + tr("Error: %1").arg(errorMessage); }
        ui->statusLabel->setText(text);
        ui->statusLabel->setStyleSheet("color: orange;");
    }
    ui->cancelButton->setText(tr("Close"));
    ui->currentTextLabel->clear();

    //表示する内容が無ければ、結果を少し表示してから閉じる
    if (successCount == totalCount && hasMessages() == false) {
        QTimer::singleShot(1000, this, [this]() {
            if (!isCancelled) {
                accept();
            }
        });
    }
}

void TranslationProgressDialog::onCancelClicked()
{
    if (ui->cancelButton->text() == tr("Cancel")) {
        isCancelled = true;
        emit cancelled();
        //中止までに発生したエラー・警告は確認できるよう残す
        if (hasMessages()) {
            ui->statusLabel->setText(tr("Translation cancelled."));
            ui->cancelButton->setText(tr("Close"));
            ui->currentTextLabel->clear();
            return;
        }
        reject();
    } else {
        reject();
    }
}