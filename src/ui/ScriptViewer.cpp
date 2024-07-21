#include "ScriptViewer.h"
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QPainter>


HighlighterBase::HighlighterBase(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , colorList(ColorTypeCount)
{
    // Default colors
    colorList[KeywordColor] = QColor(0x398ecf);
    colorList[ClassColor] = Qt::darkMagenta;
    colorList[SingleLineCommentColor] = Qt::green;
    colorList[MultiLineCommentColor] = Qt::green;
    colorList[QuotationColor] = QColor(0xd69d77);
    colorList[FunctionColor] = Qt::cyan;
}

void HighlighterBase::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : std::as_const(highlightingRules)) {
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
        if(match.hasMatch() == false){ break; }
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


RubyHighlighter::RubyHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent)
    : HighlighterBase(parent)
{
    this->updateTextColor(initTheme);
}

void RubyHighlighter::updateTextColor(ColorTheme::Theme theme)
{
    if(theme == ColorTheme::Dark){
        colorList[KeywordColor] = QColor(0x398ecf).lighter();
        colorList[ClassColor] = QColor(Qt::darkMagenta).lighter();
        colorList[SingleLineCommentColor] = Qt::green;
        colorList[MultiLineCommentColor] = Qt::green;
        colorList[QuotationColor] = QColor(0xd69d77).lighter();
        colorList[FunctionColor] = Qt::cyan;
    }
    else if(theme == ColorTheme::Light){
        colorList[KeywordColor] = QColor(0x398ecf).darker();
        colorList[ClassColor] = Qt::darkMagenta;
        colorList[SingleLineCommentColor] = QColor(Qt::green).darker();
        colorList[MultiLineCommentColor] = QColor(Qt::green).darker();
        colorList[QuotationColor] = QColor(0xd69d77).darker();
        colorList[FunctionColor] = QColor(Qt::cyan).darker();
    }
    HighlightingRule rule;

    keywordFormat.setForeground(colorList[KeywordColor]);
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
    classFormat.setForeground(colorList[ClassColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    rule.format = classFormat;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(colorList[SingleLineCommentColor]);
    rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(colorList[MultiLineCommentColor]);

    quotationFormat.setForeground(colorList[QuotationColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\'.*\'"));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(colorList[FunctionColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    rule.format = functionFormat;
    highlightingRules.append(rule);

    commentStartExpression = QRegularExpression(QStringLiteral("begin"));
    commentEndExpression = QRegularExpression(QStringLiteral("end"));

    this->rehighlight();
}


JSHighlighter::JSHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent)
    : HighlighterBase(parent)
{
    this->updateTextColor(initTheme);
}

void JSHighlighter::updateTextColor(ColorTheme::Theme theme)
{
    HighlightingRule rule;

    keywordFormat.setForeground(colorList[KeywordColor]);
    keywordFormat.setFontWeight(QFont::Bold);

    QStringList keywordPatterns = {
        "await", "break", "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do", "else", "enum", "export",
        "extends", "false", "finally", "for", "function", "if", "implements",
        "import", "in", "instanceof", "interface", "let", "new", "null",
        "package", "private", "protected", "public", "return", "super",
        "switch", "this", "throw", "true", "try", "typeof", "var", "void",
        "while", "with", "yield"
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression("\\b"+pattern+"\\b");
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }


    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(colorList[ClassColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    rule.format = classFormat;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(colorList[SingleLineCommentColor]);
    rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(colorList[MultiLineCommentColor]);

    quotationFormat.setForeground(colorList[QuotationColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\'.*\'"));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(colorList[FunctionColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    rule.format = functionFormat;
    highlightingRules.append(rule);

    commentStartExpression = QRegularExpression(QStringLiteral("/*"));
    commentEndExpression = QRegularExpression(QStringLiteral("*/"));

    this->rehighlight();
}



ScriptViewer::ScriptViewer(ComponentBase *parentComponent, QWidget *parent)
    : QPlainTextEdit(parent)
    , ComponentBase(parentComponent)
    , lineNumberArea(new LineNumber(this))
    , highlighter(nullptr)
    , currentFileName("")
    , viewerColors(NumColorType)
    , currentScriptExt()
{
    this->dispatchComponentList->emplace_back(this);
    connect(this, &ScriptViewer::blockCountChanged, this, &ScriptViewer::updateLineNumAreaWidth);
    connect(this, &ScriptViewer::updateRequest, this, &ScriptViewer::updateLineNumArea);

    viewerColors[Highlight]     = QColor(0xf0f00033);
    viewerColors[LineArea]      = QColor(0x292929);
    viewerColors[LineAreaText]  = QColor(0xa0a0a0);

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



        auto ext = info.suffix();
        if(currentScriptExt != ext)
        {
            if(highlighter){
                delete highlighter;
                highlighter = nullptr;
            }
            if(ext == "rb"){
                highlighter = new RubyHighlighter(this->getColorTheme().getCurrentTheme(), this->document());
            }
            else if(ext == "js"){
                highlighter = new JSHighlighter(this->getColorTheme().getCurrentTheme(), this->document());
            }

            highlightCursor.setCharFormat(QTextCharFormat());
            highlightCursor = QTextCursor();

            currentScriptExt = ext;
        }

        this->changeColor(this->getColorTheme().getCurrentTheme());

        this->clear();
        this->setPlainText(script.readAll());

        currentFileName = info.baseName();
        this->verticalScrollBar()->setValue(0);
    }
}

void ScriptViewer::scrollWithHighlight(int row, int col, int length)
{
    QTextCharFormat fmt;
    fmt.setBackground(viewerColors[Highlight]);

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
    painter.fillRect(drawRect, viewerColors[LineArea]);

    auto textBlock = this->firstVisibleBlock();
    auto blockNum = textBlock.blockNumber();
    auto top = round(this->blockBoundingGeometry(textBlock).translated(this->contentOffset()).top());
    auto bottom = top + round(this->blockBoundingRect(textBlock).height());
    const auto fontHeight = fontMetrics().height();


    while(textBlock.isValid())
    {
        if(drawRect.bottom() < top){ break; }

        if(textBlock.isVisible() && drawRect.top() <= bottom){
            painter.setPen(viewerColors[LineAreaText]);
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

void ScriptViewer::receive(DispatchType type, const QVariantList &args)
{
    if(type == ComponentBase::ChangeColor){
        if(args.empty()){ return; }

        bool isOk = false;
        args[0].toInt(&isOk);
        if(isOk == false){ return; }

        this->changeColor(this->getColorTheme().getCurrentTheme());
    }
}

void ScriptViewer::changeColor(ColorTheme::Theme theme)
{
    if(theme == ColorTheme::Dark){
        viewerColors[Highlight]     = QColor(QRgba64::fromRgba64(0x3300f0f0));
        viewerColors[LineArea]      = QColor(0x292929);
        viewerColors[LineAreaText]  = QColor(0xa0a0a0);
    }
    else if(theme == ColorTheme::Light){
        viewerColors[Highlight]     = QColor(QRgba64::fromRgba64(0x33009090));
        viewerColors[LineArea]      = QColor(0xcfcfcf);
        viewerColors[LineAreaText]  = QColor(0x262626);
    }
    if(this->highlighter){
        this->highlighter->updateTextColor(theme);
    }
    this->lineNumberArea->update();
}
