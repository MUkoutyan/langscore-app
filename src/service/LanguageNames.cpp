#include "LanguageNames.h"
#include <QCoreApplication>

namespace langscore {

    const std::unordered_map<std::string, QString>& languageDisplayMap()
    {
        static std::unordered_map<std::string, QString> map;
        if(map.empty()) {
            // BCP-47 の主要言語サブタグに対する表示名テーブル
            map["en"] = QCoreApplication::translate("LanguageNames", "English");
            map["ja"] = QCoreApplication::translate("LanguageNames", "Japanese");
            map["de"] = QCoreApplication::translate("LanguageNames", "German");
            map["fr"] = QCoreApplication::translate("LanguageNames", "French");
            map["es"] = QCoreApplication::translate("LanguageNames", "Spanish");
            map["it"] = QCoreApplication::translate("LanguageNames", "Italian");
            map["zh"] = QCoreApplication::translate("LanguageNames", "Chinese");

            auto chinese_simpl = QCoreApplication::translate("LanguageNames", "Chinese (Simplified)");
            map["zh-cn"] = chinese_simpl;
            map["zh-hans"] = chinese_simpl;
            map["zh-hans-cn"] = chinese_simpl;
            map["zh-sg"] = chinese_simpl;

            auto chinese_trad = QCoreApplication::translate("LanguageNames", "Chinese (Traditional)");
            map["zh-tw"] = chinese_trad;
            map["zh-hant"] = chinese_trad;
            map["zh-hant-tw"] = chinese_trad;
            map["zh-hk"] = chinese_trad;
            map["zh-mo"] = chinese_trad;
            map["ko"] = QCoreApplication::translate("LanguageNames", "Korean");
            map["ru"] = QCoreApplication::translate("LanguageNames", "Russian");
            map["pt"] = QCoreApplication::translate("LanguageNames", "Portuguese");
            map["nl"] = QCoreApplication::translate("LanguageNames", "Dutch");
            map["sv"] = QCoreApplication::translate("LanguageNames", "Swedish");
            map["no"] = QCoreApplication::translate("LanguageNames", "Norwegian");
            map["da"] = QCoreApplication::translate("LanguageNames", "Danish");
            map["fi"] = QCoreApplication::translate("LanguageNames", "Finnish");
            map["pl"] = QCoreApplication::translate("LanguageNames", "Polish");
            map["cs"] = QCoreApplication::translate("LanguageNames", "Czech");
            map["hu"] = QCoreApplication::translate("LanguageNames", "Hungarian");
            map["tr"] = QCoreApplication::translate("LanguageNames", "Turkish");
            map["ar"] = QCoreApplication::translate("LanguageNames", "Arabic");
            map["he"] = QCoreApplication::translate("LanguageNames", "Hebrew");
            map["hi"] = QCoreApplication::translate("LanguageNames", "Hindi");
            map["bn"] = QCoreApplication::translate("LanguageNames", "Bengali");
            map["pa"] = QCoreApplication::translate("LanguageNames", "Punjabi");
            map["vi"] = QCoreApplication::translate("LanguageNames", "Vietnamese");
            map["id"] = QCoreApplication::translate("LanguageNames", "Indonesian");
            map["ms"] = QCoreApplication::translate("LanguageNames", "Malay");
            map["th"] = QCoreApplication::translate("LanguageNames", "Thai");
            map["el"] = QCoreApplication::translate("LanguageNames", "Greek");
            map["ro"] = QCoreApplication::translate("LanguageNames", "Romanian");
            map["sr"] = QCoreApplication::translate("LanguageNames", "Serbian");
            map["uk"] = QCoreApplication::translate("LanguageNames", "Ukrainian");
            map["bg"] = QCoreApplication::translate("LanguageNames", "Bulgarian");
            map["lt"] = QCoreApplication::translate("LanguageNames", "Lithuanian");
            map["lv"] = QCoreApplication::translate("LanguageNames", "Latvian");
            map["et"] = QCoreApplication::translate("LanguageNames", "Estonian");
            map["sk"] = QCoreApplication::translate("LanguageNames", "Slovak");
            map["sl"] = QCoreApplication::translate("LanguageNames", "Slovenian");
            map["hr"] = QCoreApplication::translate("LanguageNames", "Croatian");
            map["ca"] = QCoreApplication::translate("LanguageNames", "Catalan");
            map["gl"] = QCoreApplication::translate("LanguageNames", "Galician");
            map["eu"] = QCoreApplication::translate("LanguageNames", "Basque");
            map["mt"] = QCoreApplication::translate("LanguageNames", "Maltese");
            map["is"] = QCoreApplication::translate("LanguageNames", "Icelandic");
            map["ga"] = QCoreApplication::translate("LanguageNames", "Irish");
            map["sq"] = QCoreApplication::translate("LanguageNames", "Albanian");
            map["mk"] = QCoreApplication::translate("LanguageNames", "Macedonian");
        }
        return map;
    }

    QString languageDisplayName(const QString& localeName)
    {
        // BCP-47 形式 (例: "en", "ja", "en-US", "zh-Hant-HK") を想定
        if(localeName.isEmpty()) {
            return QString();
        }

        // 正規化: '_' を '-' に置換し、小文字化
        QString tag = localeName;
        tag.replace('_', '-');
        tag = tag.toLower();

        const auto& map = languageDisplayMap();

        // 階層的に一致を試す: フルタグ -> 右端サブタグを削って再試行 -> 主言語
        QString current = tag;
        while(!current.isEmpty()) {
            auto it = map.find(current.toStdString());
            if(it != map.end()) {
                return it->second;
            }

            int idx = current.lastIndexOf('-');
            if(idx < 0) {
                break;
            }
            current = current.left(idx);
        }

        // 見つからない場合は空文字を返して呼び出し側でフォールバックする
        return QString();
    }

} // namespace langscore
