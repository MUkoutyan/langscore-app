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


private:

    invoker* _invoker;
    bool _finishInvoke;
    size_t errorInfoIndex;

    std::mutex _mutex;

    // pendingLength: 末尾で途切れていて、次の出力と結合する必要がある文字数
    std::vector<ValidationErrorInfo> processJsonBuffer(const QString& input, qsizetype& pendingLength);
    QString _pendingText;
    void addErrorText(QString text);
};
}