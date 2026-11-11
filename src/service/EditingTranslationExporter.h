#pragma once

#include <QString>
#include <QStringList>

class settings;

namespace langscore {

//divisi側で書き出したCSVに対して、アプリ側のエディターに記述した内容を反映する。
//・writeMode が 0 の「新しい内容で上書き」の場合はアプリ側の内容で置き換える。それ以外は、アプリ側で空のセルは書き出した CSV の翻訳文を残す
//・言語ごとのフォルダに書き出す設定なら、それぞれのフォルダのCSVに反映する
//・セル内の改行は ToGameLineBreak でゲームに合わせる
//fileNames を指定した場合は、editing にある "Map001.csv" などの指定ファイルのみ反映する。
//反映したファイルの内容は、UnexportedEditingFiles で使う書き出し済みの記録に追加する。
//反映したファイルのパスを返す
QStringList ApplyEditingTranslations(const settings& setting, const QStringList& fileNames = {});

//editing にあるCSVのうち、最後に書き出したときから内容が変わったファイルの名前を返す
QStringList UnexportedEditingFiles(const settings& setting);

//VXAce のパッキング後に、書き出したCSVのうち .rvdata2 にパッキングできたものを削除する。
//パッキングの出力先は divisi と同じく Data/Translate。パッチモードの場合は言語ごとのフォルダ。
//.rvdata2 が無いCSVはゲームが読む元になるため残す。削除したファイルのパスを返す
QStringList RemovePackedCsvFiles(const settings& setting);

//翻訳テキストの改行を、同じ行の原文と同じ改行にする。
//原文に改行が無ければツクールの改行コードにする。VXAce は \r\n、MV/MZ は \n
QString ToGameLineBreak(QString text, const QString& original, bool isVXAce);

} // namespace langscore
