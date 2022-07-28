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
    , lineNumberArea(new LineNumber(this))
    , highlighter(new Highlighter(this->document()))
    , currentFileName("")
{

    connect(this, &ScriptViewer::blockCountChanged, this, &ScriptViewer::updateLineNumAreaWidth);
    connect(this, &ScriptViewer::updateRequest, this, &ScriptViewer::updateLineNumArea);

    this->ensureCursorVisible();
    this->setLineWrapMode(LineWrapMode::NoWrap);
    this->updateLineNumAreaWidth();
}

void ScriptViewer::showFile(QString scriptFilePath)
{
    QFileInfo info(scriptFilePath);
    if(currentFileName == info.baseName()){ return; }

    QFile script(scriptFilePath);
    if(script.open(QFile::ReadOnly | QFile::Text))
    {
        highlightCursor.setCharFormat(QTextCharFormat());
        highlightCursor = QTextCursor();

        this->clear();
        this->setPlainText(script.readAll());

        currentFileName = info.baseName();
        this->verticalScrollBar()->setValue(0);
    }
}

void ScriptViewer::scrollWithHighlight(int row, int col, int length)
{
    QTextCharFormat fmt;
    fmt.setBackground(QColor("#33f0f000"));

    highlightCursor.setCharFormat(QTextCharFormat());
    auto textBlock = this->document()->findBlockByLineNumber(qMax(0, row-1));
    highlightCursor = QTextCursor(textBlock);
    highlightCursor.setPosition(qMax(0, highlightCursor.position()+col-1),    QTextCursor::MoveAnchor);
    highlightCursor.setPosition(highlightCursor.position()+length, QTextCursor::KeepAnchor);
    highlightCursor.setCharFormat(fmt);

    this->setFocus();
    this->setTextCursor(highlightCursor);

    //カーソルの位置を基に表示箇所を中央へ移動
    //QPlainTextEditの場合、スクロールバーの値は行数準拠になる。
    auto vBar = this->verticalScrollBar();
    row -= lround(vBar->pageStep()/2);
    vBar->setValue(qMax(0, qMin(row, vBar->maximum())));

    //水平はフォントのピクセルサイズ準拠
    auto hBar = this->horizontalScrollBar();
    const auto fontWidth = fontMetrics().horizontalAdvance('A');
    auto currentColPixel = col * fontWidth;
    //表示位置がページを超過する場合のみ中央に移動させる。
    if(currentColPixel <= hBar->pageStep()){
        currentColPixel = 0;
    }
    else{
        currentColPixel -= lround(hBar->pageStep()/2);
    }
    hBar->setValue(qMax(0, qMin(currentColPixel, hBar->maximum())));

}

void ScriptViewer::drawLineArea(QPaintEvent *event)
{
    QPainter painter(lineNumberArea);
    auto drawRect = event->rect();
    painter.fillRect(drawRect, QColor("#292929"));

    auto textBlock = this->firstVisibleBlock();
    auto blockNum = textBlock.blockNumber();
    auto top = round(this->blockBoundingGeometry(textBlock).translated(this->contentOffset()).top());
    auto bottom = top + round(this->blockBoundingRect(textBlock).height());
    const auto fontHeight = fontMetrics().height();


    while(textBlock.isValid())
    {
        if(drawRect.bottom() < top){ break; }

        if(textBlock.isVisible() && drawRect.top() <= bottom){
            painter.setPen(QColor("#a0a0a0"));
            //widthを少し小さくしてマージンにする
            painter.drawText(0, top, lineNumberArea->width()-6, fontHeight, Qt::AlignRight, QString::number(blockNum+1));
        }

        textBlock = textBlock.next();
        top = bottom;
        bottom = top + round(this->blockBoundingRect(textBlock).height());
        ++blockNum;
    }
}

int ScriptViewer::lineNumAreaWidth() const
{
    int lines = blockCount();
    int digit = 1;
    while(lines >= 10){
        lines /= 10;
        digit++;
    }
    const int margin = 16;
    return fontMetrics().horizontalAdvance('0') * qMax(1, digit) + margin;
}

void ScriptViewer::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    auto r = this->contentsRect();
    lineNumberArea->setGeometry(QRect{r.left(), r.top(), lineNumAreaWidth(), r.height()});
}

void ScriptViewer::updateLineNumArea(const QRect& rect, int deltaY)
{
    if(deltaY){
        lineNumberArea->scroll(0, deltaY);
    }
    else {
        lineNumberArea->update(0, rect.y(), lineNumberArea->width(), rect.height());
    }

    if(rect.contains(this->viewport()->rect())){
        updateLineNumAreaWidth();
    }
}

void ScriptViewer::updateLineNumAreaWidth() {
    setViewportMargins(lineNumAreaWidth(), 0, 0, 0);
}
