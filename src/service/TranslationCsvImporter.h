#pragma once

#include <QHash>
#include <QString>
#include <functional>

class QAbstractItemModel;

namespace langscore {

//外部エディターで編集した翻訳のCSV（original 列と言語の列）を、原文が一致するモデルの行へ取り込む。
//・空のセルは取り込まない（エディター側の翻訳を残す）
//・モデルに無い言語の列は取り込まない
//・セルは setData で書き換えるため、モデルの「変更あり」の扱いになる
//languageColumns は 言語名 → モデルの列。書き換えたセルの数を返す。CSVを読めない場合は -1
//positionOfRow は original 列が原文ではなく位置（旧形式の Scripts.csv の "ファイル名:行:列"）の場合の照合用
int ImportTranslationCsv(
    const QString& csvPath,
    QAbstractItemModel& model,
    const std::function<QString(int row)>& originalOfRow,
    const QHash<QString, int>& languageColumns,
    const std::function<QString(int row)>& positionOfRow = {}
);

} // namespace langscore
