#pragma once

#include <QString>
#include <unordered_map>

namespace langscore {

// ロケール名（例: "en", "ja", "en_US"）を受け取り、英語ベースの表示名（翻訳済み）を返す。
// 見つからなければ空文字を返す。呼び出し側でフォールバック表示を行ってください。
QString languageDisplayName(const QString& localeName);

// 生のマップ参照が必要な場合に使用可能
const std::unordered_map<std::string, QString>& languageDisplayMap();

} // namespace langscore
