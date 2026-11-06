#pragma once

#include <QString>
#include <QStringList>

class settings;

namespace langscore {

//divisi側で書き出したCSVに対して、アプリ側のエディターに記述した内容を反映する。
//・writeMode が 0（新しい内容で上書き）の場合はアプリ側の内容で置き換える。それ以外はアプリ側で空の訳を残す
//・言語ごとのフォルダに書き出す設定なら、それぞれのフォルダのCSVに反映する
//・セル内の改行はゲームに合わせる（ToGameLineBreak）
//fileNames を指定した場合は、editing にあるそのファイル（"Map001.csv" など）だけを反映する。
//反映したファイルの内容は書き出し済みとして記録する（UnexportedEditingFiles）。
//反映したファイルのパスを返す
QStringList ApplyEditingTranslations(const settings& setting, const QStringList& fileNames = {});

//editing にあるCSVのうち、最後に書き出したときから内容が変わったファイルの名前を返す
QStringList UnexportedEditingFiles(const settings& setting);

//VXAce のパッキング後に、書き出したCSVのうち .rvdata2 にパッキングできたものを削除する。
//パッキングの出力先は divisi と同じく Data/Translate（パッチモードは言語ごとのフォルダ）。
//.rvdata2 が無いCSVはゲームが読む元になるため残す。削除したファイルのパスを返す
QStringList RemovePackedCsvFiles(const settings& setting);

//翻訳テキストの改行を、同じ行の原文と同じ改行にする。
//原文に改行が無ければツクールの改行（VXAce は \r\n、MV/MZ は \n）にする
QString ToGameLineBreak(QString text, const QString& original, bool isVXAce);

} // namespace langscore
