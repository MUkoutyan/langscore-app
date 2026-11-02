#pragma once

#include <QString>
#include <QStringList>
#include <QFile>
#include <algorithm>
#include <vector>
#include "text_position.h"


namespace langscore
{

static std::vector<QString> parseCsvLine(QString line)
{
    QString tmp = "";
    bool find_dq = false;
    std::vector<QString> cols;
    for(auto c : line)
    {
        if(c == '\"'){ find_dq = !find_dq; }

        if(find_dq){
            tmp.push_back(c);
            continue;
        }

        if(c == ','){
            find_dq = false;
            cols.emplace_back(std::move(tmp));
            tmp = "";
            continue;
        }
        else if(c == '\n'){
            find_dq = false;
            cols.emplace_back(std::move(tmp));
            tmp = "";
        }
        tmp.push_back(c);
    }
    //末尾が空の列（"a," の2列目）も1列として扱う
    if(line.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

    return cols;
}

static std::vector<std::vector<QString>> readCsv(QString path)
{
    //std::ifstream に toLocal8Bit() のパスを渡すと ANSI コードページ外の文字を含むパスを開けないため QFile を使う
    QFile file(path);
    if(file.open(QIODevice::ReadOnly | QIODevice::Text) == false){ return {}; }

    auto lines = QString::fromUtf8(file.readAll()).split('\n');
    if(lines.isEmpty() == false && lines.last().isEmpty()){ lines.removeLast(); }

    std::vector<QString> rows;
    QString line_buffer;
    for(const auto& line : lines)
    {
        auto num_dq = std::count(line.begin(), line.end(), '\"');

        if(line_buffer.isEmpty() == false){
            num_dq += 1;
        }

        if(num_dq % 2 == 0){
            line_buffer += line;
            rows.emplace_back(std::move(line_buffer));
            line_buffer = "";
        }
        else{
            line_buffer += line+'\n';	//getlineに改行は含まれないのでここで足す
        }
    }

    std::vector<std::vector<QString>> csv;
    for(auto& row : rows)
    {
        bool find_dq = false;
        bool reachedLineEnd = false;
        QString tmp = "";
        std::vector<QString> cols;
        for(auto c : row)
        {
            if(c == '\"'){ find_dq = !find_dq; }

            if(find_dq){
                tmp.push_back(c);
                continue;
            }

            if(c == ','){
                find_dq = false;
                cols.emplace_back(std::move(tmp));
                tmp = "";
                continue;
            }
            else if(c == '\n'){
                find_dq = false;
                cols.emplace_back(std::move(tmp));
                tmp = "";
                reachedLineEnd = true;
                break;
            }
            tmp.push_back(c);
        }
        //末尾が空の列（翻訳が未入力の列）も1列として扱う
        if(reachedLineEnd == false && row.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

        csv.emplace_back(std::move(cols));
    }

    if(csv.size() < 2){ return {}; }

    return csv;
}

static ScriptTextPosition parseScriptNameWithRowCol(QString script)
{
    auto colStart = script.lastIndexOf(":");
    auto colStr = script.mid(colStart+1, script.size()-colStart);
    bool colOk = false;
    auto col = colStr.toUInt(&colOk);
    script.remove(colStart, script.size()-colStart);
    auto rowStart = script.lastIndexOf(":");
    auto rowStr = script.mid(rowStart+1, colStart-rowStart);
    bool rowOk = false;
    auto row = rowStr.toUInt(&rowOk);

    ScriptTextPosition result;
    if(rowOk && colOk){
        //fileName:row:colの形式でパースできた場合の処理
        //result.scriptFileName = script.remove(rowStart, script.size()-rowStart);
        result.type = ScriptTextPosition::Type::RowCol;
        auto cell = ScriptTextPosition::RowCol{};
        cell.col = col; cell.row = row;
        result.d = cell;
    }
    else{
        //それ以外はfileName:変数名として認識する。
        //既に文字列を分割しているので、該当する変数を代入。
        result.type = ScriptTextPosition::Type::Argument;
        //result.scriptFileName = rowStr;
        auto cell = ScriptTextPosition::ScriptArg{};
        cell.valueName = colStr;
        result.d = cell;
    }

    return result;
}

static std::tuple<size_t, size_t> parseScriptWithRowCol(QString script)
{
    auto list = script.split(":");
    constexpr size_t invalid = std::numeric_limits<size_t>::max();
    if(list.size() < 2){ return std::forward_as_tuple(invalid, invalid); }

    bool isOk = false;
    auto row = list[0].toLongLong(&isOk);
    if(isOk == false) {
        return std::forward_as_tuple(invalid, invalid);
    }

    auto col = list[1].toLongLong(&isOk);
    if(isOk == false) {
        return std::forward_as_tuple(invalid, invalid);
    }

    return std::forward_as_tuple(row, col);
}

}
