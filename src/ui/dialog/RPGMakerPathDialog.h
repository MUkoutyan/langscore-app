#pragma once
#include <QDialog>
#include <QStringList>
#include "../../settings.h"

class QLineEdit;

//テストプレイに使う MV・MZ のツクールのインストール先を指定するダイアログ
class RPGMakerPathDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RPGMakerPathDialog(QWidget* parent = nullptr);

    //設定に保存したツクールのインストール先を返す
    static QString rpgMakerPath(settings::ProjectType type);

    //テストプレイを起動する実行ファイルと引数を返す
    static std::pair<QString, QStringList> testPlayCommand(settings::ProjectType type, const QString& gameProjectPath, const QString& rpgMakerPath);

private:
    QLineEdit* mvPathEdit;
    QLineEdit* mzPathEdit;
};
