#include "ScriptViewer.h"
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QDebug>
#include <QPainter>

/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/


//! [0]
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
//! [0] //! [1]
    }
//! [1]

//! [2]
    classFormat.setFontWeight(QFont::Bold);
    classFormat.setForeground(Qt::darkMagenta);
    rule.pattern = QRegularExpression(QStringLiteral("\\bQ[A-Za-z]+\\b"));
    rule.format = classFormat;
    highlightingRules.append(rule);
//! [2]

//! [3]
    singleLineCommentFormat.setForeground(Qt::green);
    rule.pattern = QRegularExpression(QStringLiteral("#[^\n]*"));
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    multiLineCommentFormat.setForeground(Qt::green);
//! [3]

//! [4]
    quotationFormat.setForeground(QColor("#d69d77"));
    rule.pattern = QRegularExpression(QStringLiteral("\".*\""));
    rule.format = quotationFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression(QStringLiteral("\'.*\'"));
    rule.format = quotationFormat;
    highlightingRules.append(rule);
//! [4]

//! [5]
    functionFormat.setFontItalic(true);
    functionFormat.setForeground(Qt::cyan);
    rule.pattern = QRegularExpression(QStringLiteral("\\b[A-Za-z0-9_]+(?=\\()"));
    rule.format = functionFormat;
    highlightingRules.append(rule);
//! [5]

//! [6]
    commentStartExpression = QRegularExpression(QStringLiteral("begin"));
    commentEndExpression = QRegularExpression(QStringLiteral("end"));
}
//! [6]

//! [7]
void Highlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
//! [7] //! [8]
    setCurrentBlockState(0);
//! [8]

//! [9]
    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = text.indexOf(commentStartExpression);

//! [9] //! [10]
    while (startIndex >= 0) {
//! [10] //! [11]
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
//! [11]


ScriptViewer::ScriptViewer(QWidget *parent)
    : QTextEdit(parent)
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
        highlightCursor.setCharFormat(QTextCharFormat());
        highlightCursor = QTextCursor();

        this->clear();
        this->setText(script.readAll());

        currentFileName = info.baseName();
        this->verticalScrollBar()->setValue(0);


        qDebug() << this->verticalScrollBar()->minimum();
        qDebug() << this->verticalScrollBar()->maximum();
        qDebug() << this->verticalScrollBar()->pageStep();
    }
}

void ScriptViewer::scrollWithHighlight(int row, int col, int length)
{
    QTextCharFormat fmt;
    fmt.setBackground(QColor("#33ffffff"));

//    this->textCursor().clearSelection();

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
