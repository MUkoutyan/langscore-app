#pragma once

#include "src/ui/ComponentBase.h"
#include <QTextEdit>
#include <QPlainTextEdit>

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QStringList>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class HighlighterBase : public QSyntaxHighlighter
{
public:
    enum ColorType {
        KeywordColor,
        ClassColor,
        SingleLineCommentColor,
        MultiLineCommentColor,
        QuotationColor,
        NumberColor,
        FunctionColor,
        ColorTypeCount
    };

    HighlighterBase(QTextDocument *parent);
    virtual void updateTextColor(ColorTheme::Theme theme) = 0;
    void setUserClassNames(const QStringList& classNames);
protected:
    void highlightBlock(const QString &text) override;

    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
        int captureGroup = 0;
    };
    QList<HighlightingRule> highlightingRules;
    QVector<QColor> colorList;

    QRegularExpression commentStartExpression;
    QRegularExpression commentEndExpression;
    QStringList userClassNames;

    virtual void rebuildHighlightingRules() = 0;

    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat functionFormat;
};

class RubyHighlighter : public HighlighterBase
{
    Q_OBJECT
public:
    RubyHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent = nullptr);
    void updateTextColor(ColorTheme::Theme theme) override;
protected:
    void rebuildHighlightingRules() override;
};

class JSHighlighter : public HighlighterBase
{
    Q_OBJECT
public:
    JSHighlighter(ColorTheme::Theme initTheme, QTextDocument *parent = nullptr);
    void updateTextColor(ColorTheme::Theme theme) override;
protected:
    void rebuildHighlightingRules() override;
};

class SearchBar : public QWidget
{
    Q_OBJECT
public:
    explicit SearchBar(QWidget* parent = nullptr);

    QString currentText() const;
    void setMatchInfo(int current, int total);
    void focusInput();
    void setSearchText(const QString& text);

signals:
    void searchForward();
    void searchBackward();
    void closed();
    void textChanged(const QString& text);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QLineEdit* searchInput;
    QLabel* matchCountLabel;
    QPushButton* prevButton;
    QPushButton* nextButton;
    QPushButton* closeButton;
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
    void setHighlightedClassNames(const QStringList& classNames);

    void showSearchBar();
    void hideSearchBar();

    void drawLineArea(QPaintEvent* event);
    int lineNumAreaWidth() const;

    void updateSearchHighlights(const QString& keyword);

private:

    enum ViewerColorType {
        Highlight,
        LineArea,
        LineAreaText,
        SearchHighlight,
        SearchCurrentHighlight,
        NumColorType
    };

    void resizeEvent(QResizeEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

    void updateLineNumArea(const QRect& rect, int dy);
    void updateLineNumAreaWidth();
    void syncHighlightedClassNames();

    void receive(DispatchType type, const QVariantList& args) override;

    void changeColor(ColorTheme::Theme theme);
    void searchNext();
    void searchPrev();

    QWidget* lineNumberArea;
    HighlighterBase* highlighter;
    QString currentFileName;
    QTextCursor highlightCursor;
    QVector<QColor> viewerColors;
    QString currentScriptExt;
    SearchBar* searchBar;
    QString searchKeyword;
    int currentSearchIndex;
    QList<QTextEdit::ExtraSelection> searchSelections;
    QStringList highlightedClassNames;

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

