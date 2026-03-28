#pragma once

#include <QString>

struct ValidationErrorInfo
{
    enum ErrorType { Error, Warning, Invalid };
    enum ErrorSummary {
        None = -1,          //エラーなし
        EmptyCol = 0,       //翻訳文が空
        NotFoundEsc,        //原文にある制御文字が翻訳文に含まれていない
        UnclosedEsc,        //[]で閉じる必要のある制御文字が閉じられていない
        IncludeCR,          //翻訳文にCR改行が含まれている。(マップのみ検出)
        NotEQLang,          //設定した言語とCSVを内の言語列に差異がある
        PartiallyClipped,   //テキストの最後の文字が少し見切れる
        FullyClipped,       //完全に見えない文字がある
        OverTextCount,      //テキストの文字数が多すぎる
        InvalidCSV,         //CSVファイルが不正

        Max
    };

    QString filePath;
    ErrorType type = Invalid;
    ErrorSummary summary = None;
    size_t row = 0;
    int width = 0;
    QString language;
    QString detail;
    size_t id = 0;
    bool shown = false;
};