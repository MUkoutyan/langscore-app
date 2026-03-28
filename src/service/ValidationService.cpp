#include "ValidationService.h"
#include "invoker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>

using namespace langscore;


enum ErrorTextCol
{
    Type = 0,
    Summary,
    Language,
    Details,
    File,
    Row,
};


ValidationService::ValidationService(ComponentBase* t)
    : ComponentBase(t)
    , QObject()
    , _invoker(new invoker(this))
    , errorInfoIndex(0)
{
    connect(_invoker, &invoker::getStdOut, this, &ValidationService::addErrorText);
    connect(_invoker, &invoker::finish, this, [this](int, QProcess::ExitStatus status)
    {
        if(status == QProcess::ExitStatus::CrashExit) {

        }
        this->_finishInvoke = true;
        this->dispatch(NotifyFinishValidateCSV, {});
    });
}

void ValidationService::receive(DispatchType type, const QVariantList& args)
{
    if(type == DispatchType::ValidateCSV) {
        this->_finishInvoke = false;
        _invoker->validate();
    }
}

ValidationErrorInfo ValidationService::convertErrorInfo(std::vector<QString> csvText)
{
    ValidationErrorInfo info;

    auto typeText = csvText[ErrorTextCol::Type];
    if(typeText == "Warning") {
        info.type = ValidationErrorInfo::Warning;
    }
    else if(typeText == "Error") {
        info.type = ValidationErrorInfo::Error;
    }
    else {
        info.type = ValidationErrorInfo::Invalid;
        return info;
    }

    bool isOk = false;
    auto summaryRaw = csvText[ErrorTextCol::Summary].toInt(&isOk);
    if(isOk)
    {
        if(ValidationErrorInfo::EmptyCol <= summaryRaw && summaryRaw <= ValidationErrorInfo::Max) {
            info.summary = static_cast<ValidationErrorInfo::ErrorSummary>(summaryRaw);
        }
        else {
            info.summary = ValidationErrorInfo::None;
            info.type = ValidationErrorInfo::Invalid;
            return info;
        }
    }

    info.language = csvText[ErrorTextCol::Language];
    info.detail = csvText[ErrorTextCol::Details];
    info.row = csvText[ErrorTextCol::Row].toULongLong();
    info.id = (++errorInfoIndex);   //1開始にする

    return info;
}


std::vector<ValidationErrorInfo> ValidationService::processJsonBuffer(const QString& input)
{
    std::vector<ValidationErrorInfo> errors;
    int pos = 0;
    const int len = input.length();

    // 入力全体を走査
    while(pos < len)
    {
        // JSON オブジェクトは '{' で始まると仮定
        int start = input.indexOf('{', pos);
        if(start == -1)
            break;  // '{' が見つからなければ終了

        // '{' から対応する '}' を探す
        int depth = 0;
        bool inString = false;  // ダブルクォート内かどうか
        bool escape = false;    // エスケープ中かどうか
        int end = -1;
        for(int i = start; i < len; ++i)
        {
            QChar ch = input.at(i);
            if(inString)
            {
                // 文字列内はエスケープ処理に注意
                if(escape)
                {
                    escape = false;
                }
                else
                {
                    if(ch == '\\')
                        escape = true;
                    else if(ch == '"')
                        inString = false;
                }
            }
            else
            {
                if(ch == '"')
                {
                    inString = true;
                }
                else if(ch == '{')
                {
                    ++depth;
                }
                else if(ch == '}')
                {
                    --depth;
                    if(depth == 0)
                    {
                        end = i;
                        break;
                    }
                }
            }
        }

        // 終端が見つからなければ、後続のデータを待つ必要があるので break
        if(end == -1) {
            break;
        }

        // 完全な JSON オブジェクトを抽出（trim() して不要な空白を除去）
        QString jsonStr = input.mid(start, end - start + 1).trimmed();

        // UTF-8 に変換してから QJsonDocument でパース
        QJsonParseError parseError;
        QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &parseError);
        if(parseError.error != QJsonParseError::NoError)
        {
            qWarning() << "JSON parse error:" << parseError.errorString() << "in:" << jsonStr;
        }
        else if(!doc.isObject())
        {
            qWarning() << "JSON is not an object:" << jsonStr;
        }
        else
        {
            QJsonObject obj = doc.object();
            ValidationErrorInfo info;
            info.id = (++errorInfoIndex);   //1開始にする

            if(obj.contains("Type"))
            {
                int type = obj.value("Type").toInt();
                if(ValidationErrorInfo::Error <= type && type < ValidationErrorInfo::Invalid) {
                    info.type = static_cast<ValidationErrorInfo::ErrorType>(type);
                }
                else {
                    info.type = ValidationErrorInfo::Invalid;
                }
            }

            if(obj.contains("Summary"))
            {
                int summary = obj.value("Summary").toInt();
                if(ValidationErrorInfo::None < summary && summary < ValidationErrorInfo::Max) {
                    info.summary = static_cast<ValidationErrorInfo::ErrorSummary>(summary);
                }
                else {
                    info.summary = ValidationErrorInfo::None;
                }
            }
            if(obj.contains("File")) {
                info.filePath = obj.value("File").toString();
            }
            if(obj.contains("Row")) {
                info.row = static_cast<size_t>(obj.value("Row").toInteger());
            }
            if(obj.contains("Width")) {
                info.width = obj.value("Width").toInt();
            }
            if(obj.contains("Language")) {
                info.language = obj.value("Language").toString();
            }
            if(obj.contains("Details")) {
                info.detail = obj.value("Details").toString();
            }


            // 1件の ErrorInfo を追加
            errors.emplace_back(std::move(info));
        }

        // 次の JSON オブジェクトの探索位置を更新
        pos = end + 1;
    }

    return errors;
}


void ValidationService::addErrorText(QString text)
{
    if(text.isEmpty()) { return; }

    auto infos = processJsonBuffer(text);
    std::vector<ValidationErrorInfo> needUpdateErrorInfos;
    for(auto&& info : infos)
    {
        QFileInfo fileInfo(info.filePath);
        auto fileName = fileInfo.baseName();
        _mutex.lock();
        this->runtimeData->errors[fileName].emplace_back(std::move(info));
        this->runtimeData->updateList[fileName] = true;
        _mutex.unlock();
    }
}
