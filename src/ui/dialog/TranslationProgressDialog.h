#pragma once

#include "PopupDialogBase.h"
#include <QTimer>

namespace Ui {
class TranslationProgressDialog;
}

class TranslationProgressDialog : public PopupDialogBase
{
    Q_OBJECT

public:
    explicit TranslationProgressDialog(QWidget *parent = nullptr);
    ~TranslationProgressDialog();

    void startProgress(int totalRequests);
    void updateProgress(int completed, int total);
    void setCurrentText(const QString& text);
    void showError(const QString& errorMessage);
    void showCompletion(int successCount, int totalCount, const QString& errorMessage = {});
    //翻訳中のエラー・警告。1件でもあれば、完了しても自動では閉じない
    void addMessage(const QString& message);
    bool hasMessages() const;

signals:
    void cancelled();

private slots:
    void onCancelClicked();

private:
    Ui::TranslationProgressDialog *ui;
    QTimer* updateTimer;
    bool isCancelled;
};