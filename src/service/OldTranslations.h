#pragma once
#include <QString>
#include <QStringList>
#include <QSet>

//原文が変更・削除された行の翻訳文を、古い翻訳として editing/old/<ファイル名>.csv に残す。
//editing/ 直下のCSVのみが検証・書き出しの対象になるため、古い翻訳は検証も書き出しもされない
namespace langscore
{
QString OldTranslationsPath(const QString& editingCsvPath);

//editing の CSV のうち、現在の原文に無い行で翻訳文がある行を古い翻訳へ移す。移した行の数を返す
int KeepOldTranslations(const QString& editingCsvPath, const QSet<QString>& currentOriginals);

//古い翻訳から、指定した原文の行を削除する。原文を指定しなければすべて削除する
bool RemoveOldTranslations(const QString& editingCsvPath, const QStringList& originals = {});

//すべてのファイルの古い翻訳を削除する
bool RemoveAllOldTranslations(const QString& editingFolder);
}
