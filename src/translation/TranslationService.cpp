#include "TranslationService.h"
#include <QRegularExpression>

TranslationService::TranslationService(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
{
}

QString TranslationService::normalizeLanguageCode(const QString& langCode)
{
    QString normalized = langCode.toLower().trimmed();
    
    // Remove common prefixes/suffixes
    QRegularExpression re("^(lang_|language_|tr_|trans_)|(_(lang|language|tr|trans))$");
    normalized.remove(re);
    
    // Handle common language code mappings
    static const QMap<QString, QString> langMappings = {
        {"en", "en"},
        {"english", "en"},
        {"jp", "ja"},
        {"jpn", "ja"},
        {"japanese", "ja"},
        {"ja", "ja"},
        {"zh", "zh"},
        {"chinese", "zh"},
        {"cn", "zh"},
        {"zh-cn", "zh"},
        {"zh_cn", "zh"},
        {"simplified chinese", "zh"},
        {"zh-tw", "zh-TW"},
        {"zh_tw", "zh-TW"},
        {"traditional chinese", "zh-TW"},
        {"de", "de"},
        {"german", "de"},
        {"fr", "fr"},
        {"french", "fr"},
        {"es", "es"},
        {"spanish", "es"},
        {"it", "it"},
        {"italian", "it"},
        {"pt", "pt"},
        {"portuguese", "pt"},
        {"ru", "ru"},
        {"russian", "ru"},
        {"ko", "ko"},
        {"korean", "ko"}
    };
    
    if (langMappings.contains(normalized)) {
        return langMappings[normalized];
    }
    
    return normalized;
}

QString TranslationService::GetTranslationLanguageCode(TranslationService::ServiceType serviceType, QString languageName)
{
    // Define language mappings for different services
    static const QMap<QString, QMap<TranslationService::ServiceType, QString>> langServiceMap = {
        // English
        {"english",  {{TranslationService::ServiceType::DeepL, "EN"}, {TranslationService::ServiceType::GoogleTranslate, "en"}}},
        {"en",       {{TranslationService::ServiceType::DeepL, "EN"}, {TranslationService::ServiceType::GoogleTranslate, "en"}}},

        // Japanese
        {"japanese", {{TranslationService::ServiceType::DeepL, "JA"}, {TranslationService::ServiceType::GoogleTranslate, "ja"}}},
        {"jp",       {{TranslationService::ServiceType::DeepL, "JA"}, {TranslationService::ServiceType::GoogleTranslate, "ja"}}},
        {"ja",       {{TranslationService::ServiceType::DeepL, "JA"}, {TranslationService::ServiceType::GoogleTranslate, "ja"}}},

        // Chinese (Simplified)
        {"chinese",  {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        {"zh",       {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        {"cn",       {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},
        {"zh-cn",    {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-CN"}}},
        {"zh_cn",    {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-CN"}}},
        {"simplified chinese", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh"}}},

        // Chinese (Traditional)
        {"traditional chinese", {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-TW"}}},
        {"zh-tw",    {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-TW"}}},
        {"zh_tw",    {{TranslationService::ServiceType::DeepL, "ZH"}, {TranslationService::ServiceType::GoogleTranslate, "zh-TW"}}},

        // German
        {"german",   {{TranslationService::ServiceType::DeepL, "DE"}, {TranslationService::ServiceType::GoogleTranslate, "de"}}},
        {"de",       {{TranslationService::ServiceType::DeepL, "DE"}, {TranslationService::ServiceType::GoogleTranslate, "de"}}},

        // French
        {"french",   {{TranslationService::ServiceType::DeepL, "FR"}, {TranslationService::ServiceType::GoogleTranslate, "fr"}}},
        {"fr",       {{TranslationService::ServiceType::DeepL, "FR"}, {TranslationService::ServiceType::GoogleTranslate, "fr"}}},

        // Spanish
        {"spanish",  {{TranslationService::ServiceType::DeepL, "ES"}, {TranslationService::ServiceType::GoogleTranslate, "es"}}},
        {"es",       {{TranslationService::ServiceType::DeepL, "ES"}, {TranslationService::ServiceType::GoogleTranslate, "es"}}},

        // Italian
        {"italian",  {{TranslationService::ServiceType::DeepL, "IT"}, {TranslationService::ServiceType::GoogleTranslate, "it"}}},
        {"it",       {{TranslationService::ServiceType::DeepL, "IT"}, {TranslationService::ServiceType::GoogleTranslate, "it"}}},

        // Portuguese
        {"portuguese", {{TranslationService::ServiceType::DeepL, "PT"}, {TranslationService::ServiceType::GoogleTranslate, "pt"}}},
        {"pt",       {{TranslationService::ServiceType::DeepL, "PT"}, {TranslationService::ServiceType::GoogleTranslate, "pt"}}},

        // Russian
        {"russian",  {{TranslationService::ServiceType::DeepL, "RU"}, {TranslationService::ServiceType::GoogleTranslate, "ru"}}},
        {"ru",       {{TranslationService::ServiceType::DeepL, "RU"}, {TranslationService::ServiceType::GoogleTranslate, "ru"}}},

        // Korean
        {"korean",   {{TranslationService::ServiceType::DeepL, "KO"}, {TranslationService::ServiceType::GoogleTranslate, "ko"}}},
        {"ko",       {{TranslationService::ServiceType::DeepL, "KO"}, {TranslationService::ServiceType::GoogleTranslate, "ko"}}},

        // Dutch
        {"dutch",    {{TranslationService::ServiceType::DeepL, "NL"}, {TranslationService::ServiceType::GoogleTranslate, "nl"}}},
        {"nl",       {{TranslationService::ServiceType::DeepL, "NL"}, {TranslationService::ServiceType::GoogleTranslate, "nl"}}},

        // Polish
        {"polish",   {{TranslationService::ServiceType::DeepL, "PL"}, {TranslationService::ServiceType::GoogleTranslate, "pl"}}},
        {"pl",       {{TranslationService::ServiceType::DeepL, "PL"}, {TranslationService::ServiceType::GoogleTranslate, "pl"}}},

        // Swedish
        {"swedish",  {{TranslationService::ServiceType::DeepL, "SV"}, {TranslationService::ServiceType::GoogleTranslate, "sv"}}},
        {"sv",       {{TranslationService::ServiceType::DeepL, "SV"}, {TranslationService::ServiceType::GoogleTranslate, "sv"}}},

        // Danish
        {"danish",   {{TranslationService::ServiceType::DeepL, "DA"}, {TranslationService::ServiceType::GoogleTranslate, "da"}}},
        {"da",       {{TranslationService::ServiceType::DeepL, "DA"}, {TranslationService::ServiceType::GoogleTranslate, "da"}}},

        // Norwegian
        {"norwegian", {{TranslationService::ServiceType::DeepL, "NB"}, {TranslationService::ServiceType::GoogleTranslate, "no"}}},
        {"no",       {{TranslationService::ServiceType::DeepL, "NB"}, {TranslationService::ServiceType::GoogleTranslate, "no"}}},
        {"nb",       {{TranslationService::ServiceType::DeepL, "NB"}, {TranslationService::ServiceType::GoogleTranslate, "no"}}},

        // Finnish
        {"finnish",  {{TranslationService::ServiceType::DeepL, "FI"}, {TranslationService::ServiceType::GoogleTranslate, "fi"}}},
        {"fi",       {{TranslationService::ServiceType::DeepL, "FI"}, {TranslationService::ServiceType::GoogleTranslate, "fi"}}},

        // Czech
        {"czech",    {{TranslationService::ServiceType::DeepL, "CS"}, {TranslationService::ServiceType::GoogleTranslate, "cs"}}},
        {"cs",       {{TranslationService::ServiceType::DeepL, "CS"}, {TranslationService::ServiceType::GoogleTranslate, "cs"}}},

        // Hungarian
        {"hungarian", {{TranslationService::ServiceType::DeepL, "HU"}, {TranslationService::ServiceType::GoogleTranslate, "hu"}}},
        {"hu",       {{TranslationService::ServiceType::DeepL, "HU"}, {TranslationService::ServiceType::GoogleTranslate, "hu"}}},

        // Romanian
        {"romanian", {{TranslationService::ServiceType::DeepL, "RO"}, {TranslationService::ServiceType::GoogleTranslate, "ro"}}},
        {"ro",       {{TranslationService::ServiceType::DeepL, "RO"}, {TranslationService::ServiceType::GoogleTranslate, "ro"}}},

        // Slovak
        {"slovak",   {{TranslationService::ServiceType::DeepL, "SK"}, {TranslationService::ServiceType::GoogleTranslate, "sk"}}},
        {"sk",       {{TranslationService::ServiceType::DeepL, "SK"}, {TranslationService::ServiceType::GoogleTranslate, "sk"}}},

        // Slovenian
        {"slovenian", {{TranslationService::ServiceType::DeepL, "SL"}, {TranslationService::ServiceType::GoogleTranslate, "sl"}}},
        {"sl",       {{TranslationService::ServiceType::DeepL, "SL"}, {TranslationService::ServiceType::GoogleTranslate, "sl"}}},

        // Bulgarian
        {"bulgarian", {{TranslationService::ServiceType::DeepL, "BG"}, {TranslationService::ServiceType::GoogleTranslate, "bg"}}},
        {"bg",       {{TranslationService::ServiceType::DeepL, "BG"}, {TranslationService::ServiceType::GoogleTranslate, "bg"}}},

        // Estonian
        {"estonian", {{TranslationService::ServiceType::DeepL, "ET"}, {TranslationService::ServiceType::GoogleTranslate, "et"}}},
        {"et",       {{TranslationService::ServiceType::DeepL, "ET"}, {TranslationService::ServiceType::GoogleTranslate, "et"}}},

        // Latvian
        {"latvian",  {{TranslationService::ServiceType::DeepL, "LV"}, {TranslationService::ServiceType::GoogleTranslate, "lv"}}},
        {"lv",       {{TranslationService::ServiceType::DeepL, "LV"}, {TranslationService::ServiceType::GoogleTranslate, "lv"}}},

        // Lithuanian
        {"lithuanian", {{TranslationService::ServiceType::DeepL, "LT"}, {TranslationService::ServiceType::GoogleTranslate, "lt"}}},
        {"lt", {{TranslationService::ServiceType::DeepL, "LT"}, {TranslationService::ServiceType::GoogleTranslate, "lt"}}},

        // Ukrainian
        {"ukrainian", {{TranslationService::ServiceType::DeepL, "UK"}, {TranslationService::ServiceType::GoogleTranslate, "uk"}}},
        {"uk", {{TranslationService::ServiceType::DeepL, "UK"}, {TranslationService::ServiceType::GoogleTranslate, "uk"}}},

        // Turkish
        {"turkish", {{TranslationService::ServiceType::DeepL, "TR"}, {TranslationService::ServiceType::GoogleTranslate, "tr"}}},
        {"tr", {{TranslationService::ServiceType::DeepL, "TR"}, {TranslationService::ServiceType::GoogleTranslate, "tr"}}},

        // Greek
        {"greek", {{TranslationService::ServiceType::DeepL, "EL"}, {TranslationService::ServiceType::GoogleTranslate, "el"}}},
        {"el", {{TranslationService::ServiceType::DeepL, "EL"}, {TranslationService::ServiceType::GoogleTranslate, "el"}}},

        // Arabic
        {"arabic", {{TranslationService::ServiceType::DeepL, "AR"}, {TranslationService::ServiceType::GoogleTranslate, "ar"}}},
        {"ar", {{TranslationService::ServiceType::DeepL, "AR"}, {TranslationService::ServiceType::GoogleTranslate, "ar"}}},

        // Hindi
        {"hindi", {{TranslationService::ServiceType::DeepL, "HI"}, {TranslationService::ServiceType::GoogleTranslate, "hi"}}},
        {"hi", {{TranslationService::ServiceType::DeepL, "HI"}, {TranslationService::ServiceType::GoogleTranslate, "hi"}}},

        // Indonesian
        {"indonesian", {{TranslationService::ServiceType::DeepL, "ID"}, {TranslationService::ServiceType::GoogleTranslate, "id"}}},
        {"id", {{TranslationService::ServiceType::DeepL, "ID"}, {TranslationService::ServiceType::GoogleTranslate, "id"}}},

        // Malay
        {"malay", {{TranslationService::ServiceType::DeepL, "MS"}, {TranslationService::ServiceType::GoogleTranslate, "ms"}}},
        {"ms", {{TranslationService::ServiceType::DeepL, "MS"}, {TranslationService::ServiceType::GoogleTranslate, "ms"}}},

        // Thai
        {"thai", {{TranslationService::ServiceType::DeepL, "TH"}, {TranslationService::ServiceType::GoogleTranslate, "th"}}},
        {"th", {{TranslationService::ServiceType::DeepL, "TH"}, {TranslationService::ServiceType::GoogleTranslate, "th"}}},

        // Vietnamese
        {"vietnamese", {{TranslationService::ServiceType::DeepL, "VI"}, {TranslationService::ServiceType::GoogleTranslate, "vi"}}},
        {"vi", {{TranslationService::ServiceType::DeepL, "VI"}, {TranslationService::ServiceType::GoogleTranslate, "vi"}}}
    };

    if(langServiceMap.contains(languageName)) {
        const auto& serviceMap = langServiceMap[languageName];
        if(serviceMap.contains(serviceType)) {
            return serviceMap[serviceType];
        }
    }

    return "";
}