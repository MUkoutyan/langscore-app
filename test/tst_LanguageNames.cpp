#include "test_common.h"
#include "LanguageNames.h"

TEST(LanguageNames, KnownLanguages)
{
    const QStringList codes = {"en", "ja", "de", "fr", "zh", "ko", "zh-cn", "zh-tw"};
    for(const auto& code : codes)
    {
        SCOPED_TRACE(code.toStdString());
        EXPECT_FALSE(langscore::languageDisplayName(code).isEmpty());
    }
}

TEST(LanguageNames, EmptyInput)
{
    EXPECT_EQ(langscore::languageDisplayName(""), QString());
}

TEST(LanguageNames, UnknownInput)
{
    EXPECT_EQ(langscore::languageDisplayName("xx-zz"), QString());
}

TEST(LanguageNames, UnderscoreNormalization)
{
    // "_" は "-" に正規化されるため zh_CN → zh-cn として検索される
    EXPECT_FALSE(langscore::languageDisplayName("zh_CN").isEmpty());
    EXPECT_FALSE(langscore::languageDisplayName("zh_TW").isEmpty());
}

TEST(LanguageNames, CaseInsensitive)
{
    // 大文字小文字を区別しない（toLower で正規化される）
    EXPECT_FALSE(langscore::languageDisplayName("EN").isEmpty());
    EXPECT_FALSE(langscore::languageDisplayName("JA").isEmpty());
}

TEST(LanguageNames, Bcp47Fallback)
{
    // en-US → en にフォールバックして一致
    EXPECT_FALSE(langscore::languageDisplayName("en-US").isEmpty());
    // zh-Hant-HK → zh-hant-hk → zh-hant → zh でフォールバック
    EXPECT_FALSE(langscore::languageDisplayName("zh-Hant-HK").isEmpty());
}

TEST(LanguageNames, MapNotEmpty)
{
    EXPECT_FALSE(langscore::languageDisplayMap().empty());
}
