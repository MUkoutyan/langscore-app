#include "test_common.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "service/OldTranslations.h"
#include "FastCSVContainer.h"

TEST(OldTranslationsTest, KeepAndRemove)
{
    // 現在の原文に無い行の翻訳文は古い翻訳に残り、原文と同じ文のみで翻訳文の無い行は残さないこと。
    // 選択した原文のみ・ファイルごと・すべてを削除できること
    QTemporaryDir tempDir;
    const auto editing = tempDir.filePath("editing");
    QDir().mkpath(editing);
    QFile csv(editing + "/Actors.csv");
    ASSERT_TRUE(csv.open(QFile::WriteOnly));
    csv.write("original,ja,en\nNow,Now,Now-en\nOld1,Old1,Old1-en\nOld2,Old2,Old2-en\nNoTrans,NoTrans,\n");
    csv.close();
    const auto csvPath = editing + "/Actors.csv";

    EXPECT_EQ(langscore::KeepOldTranslations(csvPath, {"Now"}), 2);
    //2回目も重複しない
    EXPECT_EQ(langscore::KeepOldTranslations(csvPath, {"Now"}), 2);
    langscore::FastCSVContainer old;
    ASSERT_TRUE(old.loadFromFile(langscore::OldTranslationsPath(csvPath)));
    ASSERT_EQ(old.rowCount(), size_t{2});
    EXPECT_EQ(old.at(0, QString("en")), "Old1-en");

    ASSERT_TRUE(langscore::RemoveOldTranslations(csvPath, {"Old1"}));
    ASSERT_TRUE(old.loadFromFile(langscore::OldTranslationsPath(csvPath)));
    ASSERT_EQ(old.rowCount(), size_t{1});
    EXPECT_EQ(old.at(0, QString("original")), "Old2");

    ASSERT_TRUE(langscore::RemoveOldTranslations(csvPath));
    EXPECT_FALSE(QFile::exists(langscore::OldTranslationsPath(csvPath)));

    langscore::KeepOldTranslations(csvPath, {"Now"});
    ASSERT_TRUE(langscore::RemoveAllOldTranslations(editing));
    EXPECT_FALSE(QDir(editing + "/old").exists());
}
