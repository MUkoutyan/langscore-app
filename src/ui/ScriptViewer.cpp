#include "ScriptViewer.h"
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QPainter>
#include <QHBoxLayout>
#include <QKeyEvent>


namespace
{
QRegularExpression buildExactWordPattern(const QStringList& names)
{
    QStringList escaped;
    escaped.reserve(names.size());
    for (const auto& name : names) {
        if (!name.isEmpty()) {
            escaped.append(QRegularExpression::escape(name));
        }
    }

    if (escaped.isEmpty()) {
        return QRegularExpression();
    }

    return QRegularExpression(
        QStringLiteral("(?:^|\\W)(") + escaped.join(QStringLiteral("|")) + QStringLiteral(")(?!\\w)"));
}

QStringList extractRubyClassNames(const QString& text)
{
    QStringList names;
    const QRegularExpression pattern(QStringLiteral("\\b(?:class|module)\\s+([A-Z][A-Za-z0-9_]*(?:::[A-Z][A-Za-z0-9_]*)*)"));
    auto matchIterator = pattern.globalMatch(text);
    while(matchIterator.hasNext()) {
        const auto match = matchIterator.next();
        const auto name = match.captured(1);
        if(!name.isEmpty()) {
            names.append(name);
        }
    }
    return names;
}

QStringList extractJavaScriptClassNames(const QString& text)
{
    QStringList names;
    const QRegularExpression pattern(QStringLiteral("\\bclass\\s+([A-Za-z_$][A-Za-z0-9_$]*)"));
    auto matchIterator = pattern.globalMatch(text);
    while(matchIterator.hasNext()) {
        const auto match = matchIterator.next();
        const auto name = match.captured(1);
        if(!name.isEmpty()) {
            names.append(name);
        }
    }
    return names;
}

QStringList normalizeClassNames(QStringList names)
{
    names.removeAll(QString());
    names.removeDuplicates();
    return names;
}
}

HighlighterBase::HighlighterBase(QTextDocument *parent)
    : QSyntaxHighlighter(parent)
    , colorList(ColorTypeCount)
{
    colorList[KeywordColor] = QColor(0x398ecf);
    colorList[ClassColor] = Qt::darkMagenta;
    colorList[SingleLineCommentColor] = Qt::green;
    colorList[MultiLineCommentColor] = Qt::green;
    colorList[QuotationColor] = QColor(0xd69d77);
    colorList[NumberColor] = QColor(0xb58900);
    colorList[FunctionColor] = Qt::cyan;
}

void HighlighterBase::setUserClassNames(const QStringList& classNames)
{
    userClassNames = normalizeClassNames(classNames);
    rebuildHighlightingRules();
}

void HighlighterBase::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : std::as_const(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            const int start = match.capturedStart(rule.captureGroup);
            const int length = match.capturedLength(rule.captureGroup);
            if(start >= 0 && length > 0) {
                setFormat(start, length, rule.format);
            }
        }
    }

    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1) {
        startIndex = text.indexOf(commentStartExpression);
    }

    while (startIndex >= 0) {
        const QRegularExpressionMatch endMatch = commentEndExpression.match(text, startIndex);
        int commentLength = 0;

        if (!endMatch.hasMatch()) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endMatch.capturedStart() - startIndex + endMatch.capturedLength();
        }

        setFormat(startIndex, commentLength, multiLineCommentFormat);

        if (!endMatch.hasMatch()) {
            break;
        }

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
        colorList[NumberColor] = QColor(0xf5c86a);
        colorList[FunctionColor] = Qt::cyan;
    }
    else if(theme == ColorTheme::Light){
        colorList[KeywordColor] = QColor(0x398ecf).darker();
        colorList[ClassColor] = Qt::darkMagenta;
        colorList[SingleLineCommentColor] = QColor(Qt::green).darker();
        colorList[MultiLineCommentColor] = QColor(Qt::green).darker();
        colorList[QuotationColor] = QColor(0xd69d77).darker();
        colorList[NumberColor] = QColor(0x8a6d1b);
        colorList[FunctionColor] = QColor(Qt::cyan).darker();
    }
    rebuildHighlightingRules();
}

void RubyHighlighter::rebuildHighlightingRules()
{
    highlightingRules.clear();

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
        rule.pattern = QRegularExpression(QStringLiteral("\\b") + QRegularExpression::escape(pattern) + QStringLiteral("\\b"));
        rule.format = keywordFormat;
        rule.captureGroup = 0;
        highlightingRules.append(rule);
    }

    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(colorList[ClassColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(?:class|module)\\s+([A-Z][A-Za-z0-9_]*(?:::[A-Z][A-Za-z0-9_]*)*)"));
    rule.format = classFormat;
    rule.captureGroup = 1;
    highlightingRules.append(rule);

    const auto userClassPattern = buildExactWordPattern(userClassNames);
    if(userClassPattern.isValid() && !userClassPattern.pattern().isEmpty()) {
        rule.pattern = userClassPattern;
        rule.format = classFormat;
        rule.captureGroup = 1;
        highlightingRules.append(rule);
    }

    numberFormat.setForeground(colorList[NumberColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)\\b"));
    rule.format = numberFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(colorList[FunctionColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()"));
    rule.format = functionFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    quotationFormat.setForeground(colorList[QuotationColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""));
    rule.format = quotationFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\'(?:\\\\.|[^\'\\\\])*\'"));
    rule.format = quotationFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(colorList[SingleLineCommentColor]);
    rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
    rule.format = singleLineCommentFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(colorList[MultiLineCommentColor]);
    commentStartExpression = QRegularExpression(QStringLiteral("^\\s*=begin\\b"));
    commentEndExpression = QRegularExpression(QStringLiteral("^\\s*=end\\b"));

    rehighlight();
}


JSHighlighter::JSHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent)
    : HighlighterBase(parent)
{
    this->updateTextColor(initTheme);
}

void JSHighlighter::updateTextColor(ColorTheme::Theme theme)
{
    if(theme == ColorTheme::Dark){
        colorList[KeywordColor] = QColor(0x398ecf).lighter();
        colorList[ClassColor] = QColor(Qt::darkMagenta).lighter();
        colorList[SingleLineCommentColor] = QColor(0x8fbc8f);
        colorList[MultiLineCommentColor] = QColor(0x8fbc8f);
        colorList[QuotationColor] = QColor(0xd69d77).lighter();
        colorList[NumberColor] = QColor(0xf5c86a);
        colorList[FunctionColor] = Qt::cyan;
    }
    else if(theme == ColorTheme::Light){
        colorList[KeywordColor] = QColor(0x398ecf).darker();
        colorList[ClassColor] = Qt::darkMagenta;
        colorList[SingleLineCommentColor] = QColor(0x2f7a2f);
        colorList[MultiLineCommentColor] = QColor(0x2f7a2f);
        colorList[QuotationColor] = QColor(0xd69d77).darker();
        colorList[NumberColor] = QColor(0x8a6d1b);
        colorList[FunctionColor] = QColor(Qt::cyan).darker();
    }
    rebuildHighlightingRules();
}

void JSHighlighter::rebuildHighlightingRules()
{
    highlightingRules.clear();

    HighlightingRule rule;

    keywordFormat.setForeground(colorList[KeywordColor]);
    keywordFormat.setFontWeight(QFont::Bold);

    const QStringList keywordPatterns = {
        "await", "break", "case", "catch", "class", "const", "continue",
        "debugger", "default", "delete", "do", "else", "enum", "export",
        "extends", "false", "finally", "for", "function", "if", "implements",
        "import", "in", "instanceof", "interface", "let", "new", "null",
        "package", "private", "protected", "public", "return", "super",
        "switch", "this", "throw", "true", "try", "typeof", "var", "void",
        "while", "with", "yield"
    };
    for (const QString &pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(QStringLiteral("\\b") + QRegularExpression::escape(pattern) + QStringLiteral("\\b"));
        rule.format = keywordFormat;
        rule.captureGroup = 0;
        highlightingRules.append(rule);
    }

    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(colorList[ClassColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\bclass\\s+([A-Za-z_$][A-Za-z0-9_$]*)"));
    rule.format = classFormat;
    rule.captureGroup = 1;
    highlightingRules.append(rule);

    const auto userClassPattern = buildExactWordPattern(userClassNames);
    if(userClassPattern.isValid() && !userClassPattern.pattern().isEmpty()) {
        rule.pattern = userClassPattern;
        rule.format = classFormat;
        rule.captureGroup = 1;
        highlightingRules.append(rule);
    }

    numberFormat.setForeground(colorList[NumberColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|\\d+(?:\\.\\d+)?(?:[eE][+-]?\\d+)?)\\b"));
    rule.format = numberFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    functionFormat.setFontItalic(true);
    functionFormat.setForeground(colorList[FunctionColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z_][A-Za-z0-9_]*(?=\\()"));
    rule.format = functionFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    quotationFormat.setForeground(colorList[QuotationColor]);
    rule.pattern = QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\""));
    rule.format = quotationFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\'(?:\\\\.|[^\'\\\\])*\'"));
    rule.format = quotationFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    singleLineCommentFormat.setForeground(colorList[SingleLineCommentColor]);
    rule.pattern = QRegularExpression(QStringLiteral("//[^\n]*"));
    rule.format = singleLineCommentFormat;
    rule.captureGroup = 0;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(colorList[MultiLineCommentColor]);
    commentStartExpression = QRegularExpression(QStringLiteral("/\\*"));
    commentEndExpression = QRegularExpression(QStringLiteral("\\*/"));

    rehighlight();
}


SearchBar::SearchBar(QWidget* parent)
    : QWidget(parent)
    , searchInput(new QLineEdit(this))
    , matchCountLabel(new QLabel("0/0", this))
    , prevButton(new QPushButton("\u2227", this))
    , nextButton(new QPushButton("\u2228", this))
    , closeButton(new QPushButton("\u00d7", this))
{
    this->setAutoFillBackground(true);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    searchInput->setPlaceholderText(tr("Search..."));
    searchInput->setFixedWidth(200);

    prevButton->setFixedSize(22, 22);
    nextButton->setFixedSize(22, 22);
    closeButton->setFixedSize(22, 22);
    matchCountLabel->setMinimumWidth(50);
    matchCountLabel->setAlignment(Qt::AlignCenter);

    layout->addWidget(searchInput);
    layout->addWidget(matchCountLabel);
    layout->addWidget(prevButton);
    layout->addWidget(nextButton);
    layout->addWidget(closeButton);
    setLayout(layout);

    connect(searchInput, &QLineEdit::textChanged, this, &SearchBar::textChanged);
    connect(prevButton, &QPushButton::clicked, this, &SearchBar::searchBackward);
    connect(nextButton, &QPushButton::clicked, this, &SearchBar::searchForward);
    connect(searchInput, &QLineEdit::returnPressed, this, &SearchBar::searchForward);
    connect(closeButton, &QPushButton::clicked, this, &SearchBar::closed);

    hide();
}

QString SearchBar::currentText() const
{
    return searchInput->text();
}

void SearchBar::setMatchInfo(int current, int total)
{
    matchCountLabel->setText(QString("%1/%2").arg(current).arg(total));
}

void SearchBar::focusInput()
{
    searchInput->setFocus();
    searchInput->selectAll();
}

void SearchBar::setSearchText(const QString& text)
{
    searchInput->setText(text);
}

void SearchBar::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_Escape)
    {
        emit closed();
        return;
    }
    if((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::ShiftModifier)
    {
        emit searchBackward();
        return;
    }
    QWidget::keyPressEvent(event);
}


ScriptViewer::ScriptViewer(ComponentBase *parentComponent, QWidget *parent)
    : QPlainTextEdit(parent)
    , ComponentBase(parentComponent)
    , lineNumberArea(new LineNumber(this))
    , highlighter(nullptr)
    , currentFileName("")
    , viewerColors(NumColorType)
    , currentScriptExt()
    , searchBar(new SearchBar(this))
    , searchKeyword("")
    , currentSearchIndex(0)
{
    this->addDispatch(this);
    this->setReadOnly(true);

    connect(this, &ScriptViewer::blockCountChanged, this, &ScriptViewer::updateLineNumAreaWidth);
    connect(this, &ScriptViewer::updateRequest, this, &ScriptViewer::updateLineNumArea);

    viewerColors[Highlight]              = QColor(0xf0f00033);
    viewerColors[LineArea]               = QColor(0x292929);
    viewerColors[LineAreaText]           = QColor(0xa0a0a0);
    viewerColors[SearchHighlight]        = QColor(255, 255, 0, 96);
    viewerColors[SearchCurrentHighlight] = QColor(255, 165, 0, 160);

    connect(searchBar, &SearchBar::textChanged, this, &ScriptViewer::updateSearchHighlights);
    connect(searchBar, &SearchBar::searchForward, this, &ScriptViewer::searchNext);
    connect(searchBar, &SearchBar::searchBackward, this, &ScriptViewer::searchPrev);
    connect(searchBar, &SearchBar::closed, this, &ScriptViewer::hideSearchBar);

    this->ensureCursorVisible();
    this->setLineWrapMode(LineWrapMode::NoWrap);
    this->updateLineNumAreaWidth();

    // タブ幅を4文字分に設定
    int tabWidth = 4 * this->fontMetrics().horizontalAdvance(' ');
    this->setTabStopDistance(tabWidth);
}

void ScriptViewer::setHighlightedClassNames(const QStringList& classNames)
{
    highlightedClassNames = normalizeClassNames(classNames);
    syncHighlightedClassNames();
}

ScriptViewer::~ScriptViewer()
{
    this->removeDispatch(this);
}

void ScriptViewer::showSearchBar()
{
    auto selectedText = this->textCursor().selectedText();
    if(selectedText.isEmpty() == false)
    {
        searchBar->setSearchText(selectedText);
    }

    auto r = this->contentsRect();
    auto barSize = searchBar->sizeHint();
    auto vScrollWidth = this->verticalScrollBar()->sizeHint().width();
    searchBar->setGeometry(r.right() - barSize.width() - vScrollWidth - 2, r.top() + 2, barSize.width(), barSize.height());
    searchBar->show();
    searchBar->raise();
    searchBar->focusInput();
}

void ScriptViewer::hideSearchBar()
{
    searchBar->hide();
    searchKeyword.clear();
    currentSearchIndex = 0;
    searchSelections.clear();
    setExtraSelections(searchSelections);
    this->setFocus();
}

void ScriptViewer::keyPressEvent(QKeyEvent* event)
{
    if(event->key() == Qt::Key_F && event->modifiers() == Qt::ControlModifier)
    {
        showSearchBar();
        return;
    }
    if(event->key() == Qt::Key_Escape && searchBar->isVisible())
    {
        hideSearchBar();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void ScriptViewer::showFile(QString scriptFilePath)
{
    QFileInfo info(scriptFilePath);
    if(currentFileName == info.baseName()){ return; }

    QFile script(scriptFilePath);
    if(script.open(QFile::ReadOnly | QFile::Text))
    {
        const auto currentTheme = this->getColorTheme().getCurrentTheme();
        auto ext = info.suffix();
        const auto content = script.readAll();
        if(currentScriptExt != ext)
        {
            if(highlighter){
                delete highlighter;
                highlighter = nullptr;
            }
            if(ext == "rb"){
                highlighter = new RubyHighlighter(currentTheme, this->document());
            }
            else if(ext == "js"){
                highlighter = new JSHighlighter(currentTheme, this->document());
            }

            highlightCursor.setCharFormat(QTextCharFormat());
            highlightCursor = QTextCursor();

            currentScriptExt = ext;
        }

        this->changeColor(currentTheme);

        this->clear();
        this->setPlainText(content);
        syncHighlightedClassNames();

        currentFileName = info.baseName();
        this->verticalScrollBar()->setValue(0);

        searchSelections.clear();
        setExtraSelections(searchSelections);
        searchKeyword.clear();
        currentSearchIndex = 0;
        searchBar->setMatchInfo(0, 0);
    }
}

void ScriptViewer::syncHighlightedClassNames()
{
    if(highlighter == nullptr) { return; }

    QStringList classNames = highlightedClassNames;
    const auto text = this->toPlainText();
    if(currentScriptExt == QStringLiteral("rb")) {
        classNames += extractRubyClassNames(text);
    }
    else if(currentScriptExt == QStringLiteral("js")) {
        classNames += extractJavaScriptClassNames(text);
    }

    highlighter->setUserClassNames(normalizeClassNames(classNames));
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

    auto barSize = searchBar->sizeHint();
    auto vScrollWidth = this->verticalScrollBar()->sizeHint().width();
    searchBar->setGeometry(r.right() - barSize.width() - vScrollWidth - 2, r.top() + 2, barSize.width(), barSize.height());
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

void ScriptViewer::updateSearchHighlights(const QString& keyword)
{
    searchKeyword = keyword;
    searchSelections.clear();
    currentSearchIndex = 0;

    if(keyword.isEmpty())
    {
        setExtraSelections(searchSelections);
        searchBar->setMatchInfo(0, 0);
        return;
    }

    QTextCharFormat allMatchFormat;
    allMatchFormat.setBackground(viewerColors[SearchHighlight]);

    auto searchCursor = QTextCursor(this->document());
    while(searchCursor.isNull() == false)
    {
        searchCursor = this->document()->find(keyword, searchCursor);
        if(searchCursor.isNull()){ break; }

        QTextEdit::ExtraSelection selection;
        selection.cursor = searchCursor;
        selection.format = allMatchFormat;
        searchSelections.append(selection);
    }

    if(searchSelections.isEmpty() == false)
    {
        auto& selection = searchSelections[currentSearchIndex];
        QTextCharFormat currentMatchFormat;
        currentMatchFormat.setBackground(viewerColors[SearchCurrentHighlight]);
        selection.format = currentMatchFormat;

        int row = selection.cursor.blockNumber();
        int col = selection.cursor.positionInBlock();
        this->scrollWithHighlight(row, col, keyword.length());
        this->ensureCursorVisible();
    }

    setExtraSelections(searchSelections);

    auto total = searchSelections.size();
    searchBar->setMatchInfo(0 < total ? 1 : 0, total);

    if(searchBar->isVisible() == false) {
        searchBar->setVisible(true);
    }
}

void ScriptViewer::searchNext()
{
    if(searchSelections.isEmpty()){ return; }

    QTextCharFormat allMatchFormat;
    allMatchFormat.setBackground(viewerColors[SearchHighlight]);
    searchSelections[currentSearchIndex].format = allMatchFormat;

    currentSearchIndex = (currentSearchIndex + 1) % searchSelections.size();

    QTextCharFormat currentMatchFormat;
    currentMatchFormat.setBackground(viewerColors[SearchCurrentHighlight]);
    searchSelections[currentSearchIndex].format = currentMatchFormat;

    setExtraSelections(searchSelections);

    this->setTextCursor(searchSelections[currentSearchIndex].cursor);
    this->ensureCursorVisible();

    searchBar->setMatchInfo(currentSearchIndex + 1, searchSelections.size());
}

void ScriptViewer::searchPrev()
{
    if(searchSelections.isEmpty()){ return; }

    QTextCharFormat allMatchFormat;
    allMatchFormat.setBackground(viewerColors[SearchHighlight]);
    searchSelections[currentSearchIndex].format = allMatchFormat;

    if(currentSearchIndex == 0){
        currentSearchIndex = searchSelections.size() - 1;
    }
    else{
        --currentSearchIndex;
    }

    QTextCharFormat currentMatchFormat;
    currentMatchFormat.setBackground(viewerColors[SearchCurrentHighlight]);
    searchSelections[currentSearchIndex].format = currentMatchFormat;

    setExtraSelections(searchSelections);

    this->setTextCursor(searchSelections[currentSearchIndex].cursor);
    this->ensureCursorVisible();

    searchBar->setMatchInfo(currentSearchIndex + 1, searchSelections.size());
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
    if(theme == ColorTheme::Dark)
    {
        viewerColors[Highlight]              = QColor(QRgba64::fromRgba64(0x3300f0f0));
        viewerColors[LineArea]               = QColor(0x292929);
        viewerColors[LineAreaText]           = QColor(0xa0a0a0);
        viewerColors[SearchHighlight]        = QColor(255, 255, 0, 80);
        viewerColors[SearchCurrentHighlight] = QColor(255, 165, 0, 160);
    }
    else if(theme == ColorTheme::Light)
    {
        viewerColors[Highlight]              = QColor(QRgba64::fromRgba64(0x33009090));
        viewerColors[LineArea]               = QColor(0xcfcfcf);
        viewerColors[LineAreaText]           = QColor(0x262626);
        viewerColors[SearchHighlight]        = QColor(255, 220, 0, 120);
        viewerColors[SearchCurrentHighlight] = QColor(255, 130, 0, 180);
    }
    if(this->highlighter){
        this->highlighter->updateTextColor(theme);
    }
    if(searchSelections.isEmpty() == false){
        updateSearchHighlights(searchKeyword);
    }
    this->lineNumberArea->update();
}
