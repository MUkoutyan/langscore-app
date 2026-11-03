#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "ValidationService.h"

// setting を参照するために公開する
struct ValidationTestComponent : public ComponentBase
{
    using ComponentBase::setting;
};

// 仕様の確認（Docs/bugfix-proposals.md [21]）。divisi は ValidateCSVNameList に指定されたファイル・フォルダの CSV を検証する
TEST(ValidationServiceTest, ValidateCsvWritesEditingFilePath) // [21]
{
    // 1ファイルだけ検証するとき、ValidateCSVNameList には editing フォルダの CSV のフルパスが書かれること
    QTemporaryDir tempDir;
    const auto gameDir = tempDir.filePath("Game");
    QDir().mkpath(gameDir);
    QFile projectFile(gameDir + "/Game.rmmzproject");
    ASSERT_TRUE(projectFile.open(QFile::WriteOnly));
    projectFile.close();

    ValidationTestComponent component;
    component.setting->setGameProjectPath(gameDir);
    langscore::ValidationService service(&component);

    //MainCSVTable の検証ボタンと同じく、編集中の CSV のフルパスを渡す
    const auto editingCsv = component.setting->langscoreProjectDirectory + "/editing/Actors.csv";
    //divisi.exe はテスト環境に無いため起動に失敗するが、config.json は起動前に保存される
    service.receive(ComponentBase::ValidateCSV, {editingCsv});
    QCoreApplication::processEvents();

    QFile configFile(component.setting->langscoreProjectDirectory + "/config.json");
    ASSERT_TRUE(configFile.open(QFile::ReadOnly));
    const auto root = QJsonDocument::fromJson(configFile.readAll()).object();
    const auto nameList = root["Validate"].toObject()["ValidateCSVNameList"].toArray();
    ASSERT_EQ(nameList.size(), 1);
    EXPECT_EQ(nameList[0].toString(), editingCsv);
}
