#include "ValidationService.h"
#include "invoker.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QDir>
#include "../utility.hpp"

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
    , _finishInvoke(true)
    , errorInfoIndex(0)
{
    connect(_invoker, &invoker::getStdOut, this, &ValidationService::addErrorText);
    connect(_invoker, &invoker::finish, this, [this](int exitCode, QProcess::ExitStatus status)
    {
        if(exitCode != invoker::SUCCESS || status == QProcess::ExitStatus::CrashExit) {
            this->dispatch(StatusMessage, {tr("Validation failed. Please check the log."), 5000});
        }
        this->_finishInvoke = true;
        //結果の1件ずつはログに出さないため、件数だけ出す
        size_t numErrors = 0, numWarnings = 0;
        for(const auto& [fileName, errors] : this->runtimeData->errors) {
            for(const auto& error : errors) {
                if(error.type == ValidationErrorInfo::Error) { ++numErrors; }
                else if(error.type == ValidationErrorInfo::Warning) { ++numWarnings; }
            }
        }
        this->dispatch(SendLog, {tr("Validation finished. Errors: %1, Warnings: %2").arg(numErrors).arg(numWarnings)});
        this->dispatch(NotifyFinishValidateCSV, {});
    });
}

void ValidationService::receive(DispatchType type, const QVariantList& args)
{
    if(type != DispatchType::ValidateCSV && type != DispatchType::ValidateAll) { return; }

    //実行中に次の検証を始めると、前の検証の出力が結果に混ざるため受け付けない
    if(this->_finishInvoke == false) { return; }

    QStringList checkFileList;
    this->_pendingText.clear();
    this->runtimeData->updateList.clear();
    if(type == DispatchType::ValidateCSV) {
        //検証するファイルの結果だけを消し、他のファイルの結果は残す
        for(const auto& var : args) {
            checkFileList.append(var.toString());
            this->runtimeData->errors.erase(QFileInfo(var.toString()).baseName());
        }
    }
    else {
        //編集表と行がそろうよう、書き出し先ではなくアプリ側エディターのCSVを検証する
        checkFileList = ValidationTargetFiles(*this->setting);
        this->runtimeData->errors.clear();
    }
    this->runtimeData->errorsRevision++;
    if(checkFileList.isEmpty()) {
        this->dispatch(NotifyFinishValidateCSV, {});
        return;
    }
    this->_finishInvoke = false;

    this->setting->validateObj.csvNameList = checkFileList;
    this->setting->saveForProject();
    _invoker->validate();
    this->setting->validateObj.csvNameList.clear();
}

//editing にあるCSVのうち、今のゲームにあり、翻訳から除外していないファイルだけを返す。
//ゲームから消したファイルや除外したファイルのCSVは editing に残るが、検証はしない
QStringList ValidationService::ValidationTargetFiles(const settings& setting)
{
    const auto editingFolder = setting.langscoreProjectDirectory + "/editing";
    const auto analyzeFolder = setting.analyzeDirectoryPath();
    QSet<QString> targetNames;
    const auto addTargets = [&](const auto& infoList) {
        for(const auto& info : infoList) {
            if(info.ignore) { continue; }
            const auto name = withoutAllExtension(info.fileName);
            if(QFile::exists(analyzeFolder + "/" + name + ".lsjson")) { targetNames.insert(name); }
        }
    };
    addTargets(setting.writeObj.basicDataInfo);
    addTargets(setting.writeObj.mapDataInfo);
    if(std::ranges::any_of(setting.writeObj.scriptInfo, [](const auto& script) { return script.isIgnore() == false; })) {
        targetNames.insert("Scripts");
    }

    QStringList files;
    for(const auto& fileInfo : QDir(editingFolder).entryInfoList({"*.csv"}, QDir::Files, QDir::Name)) {
        if(targetNames.contains(fileInfo.completeBaseName())) { files.append(fileInfo.absoluteFilePath()); }
    }
    return files;
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
    info.row = csvText[ErrorTextCol::Row].toLongLong();
    info.id = (++errorInfoIndex);   //1開始にする

    return info;
}


std::vector<ValidationErrorInfo> ValidationService::processJsonBuffer(const QString& input, qsizetype& pendingLength)
{
    std::vector<ValidationErrorInfo> errors;
    pendingLength = 0;
    int pos = 0;
    const int len = input.length();

    // 入力全体を走査
    while(pos < len)
    {
        // JSON オブジェクトは '{' で始まると仮定
        int start = input.indexOf('{', pos);
        if(start == -1) {
            break;  // '{' が見つからなければ終了
        }

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
                    if(ch == '\\') {
                        escape = true;
                    }
                    else if(ch == '"') {
                        inString = false;
                    }
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
        // divisi は1件を1行で出力するので、行の途中で途切れている場合のみ次の出力と結合するために残す
        if(end == -1) {
            if(input.indexOf('\n', start) == -1) {
                pendingLength = len - start;
            }
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
                info.row = static_cast<int64_t>(obj.value("Row").toInteger());
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

    //標準出力は任意の位置で分割されて届くため、前回途中で途切れた JSON と結合してから解析する
    const QString input = _pendingText + text;
    qsizetype pendingLength = 0;
    auto infos = processJsonBuffer(input, pendingLength);
    _pendingText = input.right(pendingLength);
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
    if(infos.empty() == false) { this->runtimeData->errorsRevision++; }

    //検証結果（JSON の行）は件数が多いとログが重くなるため、それ以外の行だけをログに出す
    QStringList logLines;
    for(const auto& line : input.left(input.size() - pendingLength).split(QChar(0x0A))) {
        const auto trimmed = line.trimmed();
        if(trimmed.isEmpty() == false && trimmed.startsWith('{') == false) { logLines.append(line); }
    }
    if(logLines.isEmpty() == false) {
        this->dispatch(DispatchType::SendLog, {logLines.join(QChar(0x0A))});
    }
}
