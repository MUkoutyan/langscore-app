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