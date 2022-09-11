﻿#pragma once

#include <QWidget>
#include <QMenu>
#include <unordered_map>
#include "ComponentBase.h"

namespace Ui {
class PackingMode;
}

class QTreeWidgetItem;
class invoker;
class PackingMode : public QWidget, public ComponentBase
{
    Q_OBJECT

public:
    explicit PackingMode(ComponentBase* setting, QWidget *parent = nullptr);
    ~PackingMode();

signals:
    void toPrevPage();

private:
    Ui::PackingMode *ui;
    invoker* _invoker;

    struct ErrorInfo
    {
        enum ErrorType{ Error, Warning, Invalid };
        enum ErrorSummary{
            EmptyCol, NotFoundEsc
        };

        ErrorType type = Warning;
        ErrorSummary summary = EmptyCol;
        size_t row = 0;
        QString language;
        QString detail;
        size_t id = 0;
        bool shown = false;
    };


    std::unordered_map<QString, std::vector<ErrorInfo>> errors;
    std::unordered_map<QString, bool> updateList;
    std::unordered_map<QString, QTreeWidgetItem*> treeTopItemList;
    std::mutex _mutex;
    QTimer* updateTimer;
    bool _finishInvoke;
    size_t errorInfoIndex;
    QString currentShowCSV;
    bool isValidate;

    QMenu* treeMenu;

    QString getCurrentSelectedItemFilePath();
    void setupCsvTable(QString filePath);
    void highlightError(QTreeWidgetItem* item);

    void showEvent(QShowEvent *event) override;

private slots:
    void validate();
    void packing();

    void addText(QString text);
    void updateTree();
};

