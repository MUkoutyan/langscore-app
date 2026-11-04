#pragma once

#include <QString>
#include <QStringList>

class settings;

namespace langscore {

//divisi側で書き出したCSVに対して、アプリ側のエディターに記述した内容を反映する。
//・writeMode が 0（新しい内容で上書き）の場合はアプリ側の内容で置き換える。それ以外はアプリ側で空の訳を残す
//・デフォルト言語の列を埋める設定なら、空のセルに原文を入れる
//・言語ごとのフォルダに書き出す設定なら、それぞれのフォルダのCSVに反映する
//・セル内の改行はゲームに合わせる（ToGameLineBreak）
//反映したファイルのパスを返す
QStringList ApplyEditingTranslations(const settings& setting);

//翻訳テキストの改行を、同じ行の原文と同じ改行にする。
//原文に改行が無ければツクールの改行（VXAce は \r\n、MV/MZ は \n）にする
QString ToGameLineBreak(QString text, const QString& original, bool isVXAce);

} // namespace langscore
