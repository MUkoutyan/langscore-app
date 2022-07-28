#pragma once

#include <QTextEdit>
#include <QPlainTextEdit>

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>


class Highlighter : public QSyntaxHighlighter
{
    Q_OBJECT

public:
    Highlighter(QTextDocument *parent = nullptr);

protected:
    void highlightBlock(const QString &text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> highlightingRules;

    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
};

class ScriptViewer : public QPlainTextEdit
{
    Q_OBJECT
public:
    ScriptViewer(QWidget *parent = nullptr);

    void showFile(QString scriptFilePath);
    void scrollWithHighlight(int row, int col, int length);
    QString GetCurrentFileName() const { return currentFileName; }

    void drawLineArea(QPaintEvent* event);
    int lineNumAreaWidth() const;

private:

    void resizeEvent(QResizeEvent* event) override;

    void updateLineNumArea(const QRect& rect, int dy);
    void updateLineNumAreaWidth();

    QWidget* lineNumberArea;
    Highlighter* highlighter;
    QString currentFileName;
    QTextCursor highlightCursor;
};


class LineNumber : public QWidget {
    Q_OBJECT
public:
    LineNumber(ScriptViewer* parent): QWidget(parent), parentEditor(parent){}

    QSize sizeHint() const override { return QSize(parentEditor->lineNumAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* event) override{
        parentEditor->drawLineArea(event);
    }

private:

    ScriptViewer* parentEditor;
};

