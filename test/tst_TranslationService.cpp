#include "test_common.h"
#include "translation/TranslationService.h"

// normalizeLanguageCode は protected なのでサブクラス経由で呼び出す
struct DummyService : public TranslationService
{
    using TranslationService::normalizeLanguageCode;
    ServiceType getServiceType() const override { return ServiceType::DeepL; }
    QString getServiceName() const override { return {}; }
    bool isConfigured() const override { return false; }
    void translateText(const TranslationRequest&) override {}
    QStringList getSupportedLanguages() const override { return {}; }
};

using ServiceType = TranslationService::ServiceType;

TEST(TranslationService, LanguageCode)
{
    struct Row { const char* label; ServiceType service; QString name; QString expected; };
    const Row rows[] = {
        {"deepl en",       ServiceType::DeepL,           "en",       "EN"},
        {"deepl japanese", ServiceType::DeepL,           "japanese", "JA"},
        {"deepl zh-tw",    ServiceType::DeepL,           "zh-tw",    "ZH-HANT"},
        {"google ja",      ServiceType::GoogleTranslate, "ja",       "ja"},
        {"google zh-cn",   ServiceType::GoogleTranslate, "zh-cn",    "zh-CN"},
        {"google zh-tw",   ServiceType::GoogleTranslate, "zh-tw",    "zh-TW"},
    };
    for(const auto& row : rows)
    {
        SCOPED_TRACE(row.label);
        EXPECT_EQ(TranslationService::GetTranslationLanguageCode(row.service, row.name), row.expected);
    }
}

TEST(TranslationService, LanguageCodeUnknown)
{
    EXPECT_EQ(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, "klingon"), QString());
    EXPECT_EQ(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, ""), QString());
}

TEST(TranslationService, Normalize)
{
    struct Row { const char* label; QString input; QString expected; };
    const Row rows[] = {
        {"lower",       "EN",       "en"},
        {"trim",        "  ja  ",   "ja"},
        {"alias",       "Japanese", "ja"},
        {"prefix",      "lang_jp",  "ja"},
        {"suffix",      "en_lang",  "en"},
        {"zh_TW",       "zh_TW",    "zh-TW"},
        {"passthrough", "xx",       "xx"},
    };
    for(const auto& row : rows)
    {
        SCOPED_TRACE(row.label);
        EXPECT_EQ(DummyService::normalizeLanguageCode(row.input), row.expected);
    }
}

// 不具合の再発防止
TEST(TranslationService, DeepLTraditionalChinese)
{
    // 繁体字は DeepL の繁体字ターゲット（ZH-HANT）に変換されること。"ZH" は簡体字になる
    EXPECT_EQ(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, "zh-tw"), QString("ZH-HANT"));
    EXPECT_EQ(TranslationService::GetTranslationLanguageCode(ServiceType::DeepL, "traditional chinese"), QString("ZH-HANT"));
}
