#pragma once

#include <QString>
#include <unordered_map>

namespace langscore {

// "en", "ja", "en_US" などのロケール名を受け取り、英語ベースの表示名を翻訳済みの状態で返す。
// 見つからなければ空文字を返す。フォールバック表示は呼び出し側で行う。
QString languageDisplayName(const QString& localeName);

// 生のマップ参照が必要な場合に使用可能
const std::unordered_map<std::string, QString>& languageDisplayMap();

} // namespace langscore
