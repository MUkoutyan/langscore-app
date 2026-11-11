#include "test_common.h"
// utility.hpp は settings::ProjectType を直接参照するため、先に settings.h が必要
#include "settings.h"
#include "utility.hpp"

TEST(Utility, WordCountAscii)
{
    EXPECT_EQ(langscore::wordCountUTF8("hello"), 5);
}

TEST(Utility, WordCountJapanese)
{
    // BMP の日本語文字は toLatin1() が 0 になり、カウント対象となる
    EXPECT_EQ(langscore::wordCountUTF8(QString::fromUtf8("あいう")), 3);
}

TEST(Utility, WordCountEmpty)
{
    EXPECT_EQ(langscore::wordCountUTF8(""), 0);
}

TEST(Utility, WordCountMixed)
{
    EXPECT_EQ(langscore::wordCountUTF8(QString("a") + QString::fromUtf8("あ")), 2);
}

TEST(Utility, GetExtensionNormal)
{
    EXPECT_EQ(langscore::getExtension("file.txt"), QString("txt"));
}

TEST(Utility, GetExtensionNoExt)
{
    EXPECT_EQ(langscore::getExtension("file"), QString());
}

TEST(Utility, GetExtensionMultiple)
{
    EXPECT_EQ(langscore::getExtension("archive.tar.gz"), QString("gz"));
}

TEST(Utility, WithoutExtensionNormal)
{
    EXPECT_EQ(langscore::withoutExtension("file.txt"), QString("file"));
}

TEST(Utility, WithoutExtensionNoExt)
{
    EXPECT_EQ(langscore::withoutExtension("file"), QString("file"));
}

TEST(Utility, WithoutAllExtensionSingle)
{
    EXPECT_EQ(langscore::withoutAllExtension("file.txt"), QString("file"));
}

TEST(Utility, WithoutAllExtensionMultiple)
{
    EXPECT_EQ(langscore::withoutAllExtension("archive.tar.gz"), QString("archive"));
}

TEST(Utility, WithoutAllExtensionNoExt)
{
    EXPECT_EQ(langscore::withoutAllExtension("file"), QString("file"));
}

TEST(Utility, GetScriptExtensionVXAce)
{
    EXPECT_EQ(langscore::GetScriptExtension(settings::VXAce), QString(".rb"));
}

TEST(Utility, GetScriptExtensionMV)
{
    EXPECT_EQ(langscore::GetScriptExtension(settings::MV), QString(".js"));
}

TEST(Utility, GetScriptExtensionMZ)
{
    EXPECT_EQ(langscore::GetScriptExtension(settings::MZ), QString(".js"));
}
