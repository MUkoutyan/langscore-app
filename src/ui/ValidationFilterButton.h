#pragma once
#include <QToolButton>
#include "ComponentBase.h"

//検証結果を種類・言語で絞り込むボタン。ファイルツリーと検証結果の一覧で使う。
//絞り込みはアプリの設定に保存し、次に起動したときも使う
class ValidationFilterButton : public QToolButton, public ComponentBase
{
    Q_OBJECT
public:
    ValidationFilterButton(ComponentBase* component, QWidget* parent);
};
