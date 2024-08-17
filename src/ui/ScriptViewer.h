#pragma once

#include "src/ui/ComponentBase.h"
#include <QTextEdit>
#include <QPlainTextEdit>

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>

class HighlighterBase : public QSyntaxHighlighter
{
public:
    enum ColorType {
        KeywordColor,
        ClassColor,
        SingleLineCommentColor,
        MultiLineCommentColor,
        QuotationColor,
        FunctionColor,
        ColorTypeCount
    };

    HighlighterBase(QTextDocument *parent);
    virtual void updateTextColor(ColorTheme::Theme theme) = 0;
protected:
    void highlightBlock(const QString &text) override;

    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QList<HighlightingRule> highlightingRules;
    QVector<QColor> colorList;

    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
};

class RubyHighlighter : public HighlighterBase
{
    Q_OBJECT
public:
    RubyHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent = nullptr);
    void updateTextColor(ColorTheme::Theme theme) override;
};

class JSHighlighter : public HighlighterBase
{
    Q_OBJECT
public:
    JSHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent = nullptr);
    void updateTextColor(ColorTheme::Theme theme) override;
};

class ScriptViewer : public QPlainTextEdit, public ComponentBase
{
    Q_OBJECT
public:
    ScriptViewer(ComponentBase* parentComponent, QWidget *parent = nullptr);
    ~ScriptViewer();

    void showFile(QString scriptFilePath);
    void scrollWithHighlight(int row, int col, int length);
    QString GetCurrentFileName() const { return currentFileName; }

    void drawLineArea(QPaintEvent* event);
    int lineNumAreaWidth() const;

private:

    enum ViewerColorType {
        Highlight,
        LineArea,
        LineAreaText,
        NumColorType
    };

    void resizeEvent(QResizeEvent* event) override;

    void updateLineNumArea(const QRect& rect, int dy);
    void updateLineNumAreaWidth();

    void receive(DispatchType type, const QVariantList& args) override;

    void changeColor(ColorTheme::Theme theme);

    QWidget* lineNumberArea;
    HighlighterBase* highlighter;
    QString currentFileName;
    QTextCursor highlightCursor;
    QVector<QColor> viewerColors;
    QString currentScriptExt;

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

