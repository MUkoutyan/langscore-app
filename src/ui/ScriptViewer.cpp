#include "ScriptViewer.h"
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QPainter>


Highlighter::Highlighter(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    keywordFormat.setForeground(QColor("#398ecf"));
    keywordFormat.setFontWeight(QFont::Bold);

    const QString keywordPatterns[] = {
        "BEGIN", "do", "next", "then", "END", "else", "nil", "true", "alias",
        "elsif", "not", "undef", "and", "end", "or", "unless", "begin", "ensure",
        "redo", "until", "break", "false", "rescue", "when", "case", "for", "retry",
        "while", "class", "if", "return", "while", "def", "in", "self", "__FILE__",
        "defined?", "module", "super", "__LINE__"
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression("\\b"+pattern+"\\b");
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(Qt::darkMagenta);
    rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    rule.format = classFormat;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(Qt::green);
    rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(Qt::green);

    quotationFormat.setForeground(QColor("#d69d77"));
    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\'.*\'"));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::cyan);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    rule.format = functionFormat;
    highlightingRules.append(rule);

    commentStartExpression = QRegularExpression(QStringLiteral("begin"));
    commentEndExpression = QRegularExpression(QStringLiteral("end"));
}

void Highlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStartExpression);

    while (startIndex >= 0) {
        QRegularExpressionMatch match = commentEndExpression.match(text, startIndex);
        int endIndex = match.capturedStart();
        int commentLength = 0;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex
                            + match.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = text.indexOf(commentStartExpression, startIndex + commentLength);
    }
}


ScriptViewer::ScriptViewer(QWidget *parent)
    : QPlainTextEdit(parent)
    , highlighter(new Highlighter(this->document()))
    , currentFileName("")
{
    this->ensureCursorVisible();
    this->setLineWrapMode(LineWrapMode::NoWrap);
}

void ScriptViewer::showFile(QString scriptFilePath)
{
    QFileInfo info(scriptFilePath);
    if(currentFileName == info.baseName()){ return; }

    QFile script(scriptFilePath);
    if(script.open(QFile::ReadOnly | QFile::Text))
    {
        this->clear();
        this->setPlainText(script.readAll());

        currentFileName = info.baseName();
    }
}

void ScriptViewer::scrollWithHighlight(int row, int col, int length)
{
    QTextCharFormat fmt;
    fmt.setBackground(QColor("#33ffffff"));

    highlightCursor.setCharFormat(QTextCharFormat());
    highlightCursor = QTextCursor(this->document()->findBlockByLineNumber(qMax(0, row-1)));
    highlightCursor.setPosition(qMax(0, highlightCursor.position()+col-1),    QTextCursor::MoveAnchor);
    highlightCursor.setPosition(highlightCursor.position()+length, QTextCursor::KeepAnchor);
    highlightCursor.setCharFormat(fmt);

    this->setFocus();
    this->setTextCursor(highlightCursor);

    //カーソルの位置を基に表示箇所を中央へ移動
    auto currentCursolHeight = this->cursorRect(highlightCursor);
    auto vBar = this->verticalScrollBar();
    vBar->setValue(vBar->value() + currentCursolHeight.top() - (this->rect().height()/2));

}
