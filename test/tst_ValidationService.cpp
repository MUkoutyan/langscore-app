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
    using ComponentBase::runtimeData;
};

namespace
{
    //検証を始められる状態のプロジェクトを作る
    void SetupProject(ValidationTestComponent& component, const QString& gameDir)
    {
        QDir().mkpath(gameDir);
        QFile projectFile(gameDir + "/Game.rmmzproject");
        ASSERT_TRUE(projectFile.open(QFile::WriteOnly));
        projectFile.close();
        component.setting->setGameProjectPath(gameDir);
    }

    QJsonArray ReadValidateNameList(const ValidationTestComponent& component)
    {
        QFile configFile(component.setting->langscoreProjectDirectory + "/config.json");
        if(configFile.open(QFile::ReadOnly) == false) { return {}; }
        const auto root = QJsonDocument::fromJson(configFile.readAll()).object();
        return root["Validate"].toObject()["ValidateCSVNameList"].toArray();
    }
}

// 仕様の確認。divisi は ValidateCSVNameList に指定されたファイル・フォルダの CSV を検証する
TEST(ValidationServiceTest, ValidateCsvWritesEditingFilePath)
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

TEST(ValidationServiceTest, ValidateAllChecksEditingFolder)
{
    // 全検証では、書き出し先ではなく editing フォルダの CSV を検証すること（編集表と行をそろえるため）
    QTemporaryDir tempDir;
    ValidationTestComponent component;
    SetupProject(component, tempDir.filePath("Game"));
    langscore::ValidationService service(&component);

    service.receive(ComponentBase::ValidateAll, {});
    QCoreApplication::processEvents();

    const auto nameList = ReadValidateNameList(component);
    ASSERT_EQ(nameList.size(), 1);
    EXPECT_EQ(nameList[0].toString(), component.setting->langscoreProjectDirectory + "/editing");
}

TEST(ValidationServiceTest, IgnoreRequestWhileRunning)
{
    // 検証の実行中に次の検証を頼んでも始めないこと。前の検証の出力が結果に混ざるため
    QTemporaryDir tempDir;
    ValidationTestComponent component;
    SetupProject(component, tempDir.filePath("Game"));
    langscore::ValidationService service(&component);

    const auto first = component.setting->langscoreProjectDirectory + "/editing/Actors.csv";
    const auto second = component.setting->langscoreProjectDirectory + "/editing/Items.csv";
    //終了の通知はイベントループを回すまで届かないので、1回目はまだ実行中の扱い
    service.receive(ComponentBase::ValidateCSV, {first});
    service.receive(ComponentBase::ValidateCSV, {second});

    auto nameList = ReadValidateNameList(component);
    ASSERT_EQ(nameList.size(), 1);
    EXPECT_EQ(nameList[0].toString(), first);

    //終わった後は受け付ける
    QCoreApplication::processEvents();
    service.receive(ComponentBase::ValidateCSV, {second});
    nameList = ReadValidateNameList(component);
    ASSERT_EQ(nameList.size(), 1);
    EXPECT_EQ(nameList[0].toString(), second);
    QCoreApplication::processEvents();
}

TEST(ValidationServiceTest, ErrorJsonSplitAcrossChunks)
{
    // divisi の標準出力が JSON の途中で分割されて届いても、エラーが失われないこと
    ValidationTestComponent component;
    langscore::ValidationService service(&component);

    const QString json = R"({"Type":0,"Summary":0,"File":"C:/data/Actors.csv","Row":3,"Language":"en","Details":""})";
    const auto half = json.size() / 2;
    service.addErrorText(json.left(half));
    service.addErrorText(json.mid(half));

    const auto& errors = component.runtimeData->errors;
    ASSERT_TRUE(errors.contains("Actors"));
    ASSERT_EQ(errors.at("Actors").size(), size_t{1});
    EXPECT_EQ(errors.at("Actors")[0].row, std::int64_t{3});
}
