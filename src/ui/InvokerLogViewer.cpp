#include "InvokerLogViewer.h"

#include <QAction>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QScrollBar>
#include <QMenu>

InvokerLogViewer::InvokerLogViewer(QWidget *parent)
    : QPlainTextEdit(parent)
    , setting(nullptr)
{
    this->setUndoRedoEnabled(false);
    this->setReadOnly(true);


    auto exportLogFile = new QAction(tr("Save log as..."), this);
    connect(exportLogFile, &QAction::triggered, this, [this](){
        //ログを名前をつけて保存
        assert(this->setting);
        auto defaultPath = this->setting->langscoreProjectDirectory + "/log.txt";
        auto path = QFileDialog::getSaveFileName(this, tr("Save log as"), defaultPath, tr("Logfile (*.txt)"));

        QFile file(path);
        if(file.open(QIODevice::WriteOnly))
        {
            file.write(this->toPlainText().toUtf8());
        }
    });

    connect(this, &QPlainTextEdit::customContextMenuRequested, this, [this, exportLogFile](const QPoint& pos){
        auto* menu = this->createStandardContextMenu();
        exportLogFile->setEnabled(this->placeholderText().isEmpty() == false);
        menu->addAction(exportLogFile);
        menu->exec(QCursor::pos());
    });
}

void InvokerLogViewer::SetSettings(std::shared_ptr<settings> setting)
{
    this->setting = std::move(setting);
}

void InvokerLogViewer::writeText(QString text)
{
    text.replace("\r\n", "\n");
    this->insertPlainText(text);
    auto vBar = this->verticalScrollBar();
    vBar->setValue(vBar->maximum());
    this->update();
}
