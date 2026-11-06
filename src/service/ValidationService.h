#pragma once

#include <vector>
#include <mutex>
#include "ValidationErrorInfo.h"
#include "ComponentBase.h"

class invoker;
namespace langscore
{
class ValidationService : public ComponentBase, public QObject
{
public:
    ValidationService(ComponentBase* t);

    void receive(DispatchType type, const QVariantList& args) override;

    ValidationErrorInfo convertErrorInfo(std::vector<QString> csvText);

    //divisi の標準出力を受け取り、検証結果に加える
    void addErrorText(QString text);

    //全体の検証で divisi に渡すCSVの一覧
    static QStringList ValidationTargetFiles(const settings& setting);

private:

    invoker* _invoker;
    bool _finishInvoke;
    size_t errorInfoIndex;

    std::mutex _mutex;

    // pendingLength: 末尾で途切れていて、次の出力と結合する必要がある文字数
    std::vector<ValidationErrorInfo> processJsonBuffer(const QString& input, qsizetype& pendingLength);
    QString _pendingText;
};
}