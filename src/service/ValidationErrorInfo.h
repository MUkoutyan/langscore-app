#pragma once

#include <QString>
#include <QObject>

struct ValidationErrorInfo
{
    enum ErrorType { Error, Warning, Invalid };
    enum ErrorSummary {
        None = -1,          //エラーなし
        EmptyCol = 0,       //翻訳文が空
        NotFoundEsc,        //原文にある制御文字が翻訳文に含まれていない
        UnclosedEsc,        //[]で閉じる必要のある制御文字が閉じられていない
        IncludeCR,          //翻訳文にCR改行が含まれている。マップのみ検出
        NotEQLang,          //設定した言語とCSV内の言語列に差異がある
        PartiallyClipped,   //テキストの最後の文字が少し見切れる
        FullyClipped,       //完全に見えない文字がある
        OverTextCount,      //テキストの文字数が多すぎる
        InvalidCSV,         //CSVファイルが不正

        Max
    };

    QString filePath;
    ErrorType type = Invalid;
    ErrorSummary summary = None;
    std::int64_t row = 0;
    int width = 0;
    QString language;
    QString detail;
    size_t id = 0;
    bool shown = false;

    QString getErrorText() const
    {
        QString text;
        switch(this->summary)
        {
        case ValidationErrorInfo::EmptyCol:
            text += QObject::tr(" Empty Text") + "[" + this->language + "]";
            break;
        case ValidationErrorInfo::NotFoundEsc:
            text += QObject::tr(" Missing Control Characters") + "[" + this->language + "] (" + this->detail + ")";
            break;
        case ValidationErrorInfo::UnclosedEsc:
            text += QObject::tr(" Unclosed Control Character") + "[" + this->language + "] (" + this->detail + ")";
            break;
        case ValidationErrorInfo::IncludeCR:
            text += QObject::tr(" Include \"\r\n\"");
            break;
        case ValidationErrorInfo::NotEQLang:
            text += QObject::tr(" The specified language does not match the language in the CSV");
            break;
        case ValidationErrorInfo::PartiallyClipped:
            text += QObject::tr(" Part of this text is cut off.");
            break;
        case ValidationErrorInfo::FullyClipped:
            text += QObject::tr(" This text is completely cut off.");
            break;
        case ValidationErrorInfo::OverTextCount:
            text += QObject::tr(" The specified number of characters has been exceeded. (%1 characters)").arg(this->width);
            break;
        case ValidationErrorInfo::InvalidCSV:
            if(this->detail.isEmpty() == true)
            {
                text += QObject::tr(" Invalid CSV. This may be caused by the text around line %1.").arg(this->row);
            }
            else
            {
                text += QObject::tr(" Invalid CSV. This may be caused by the text around %2 in line %1.").arg(this->row).arg(this->detail);
            }
            break;
        default:
            break;
        }

        return text;
    }
};