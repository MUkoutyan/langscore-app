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
    //"a," の2列目のような末尾の空の列も1列として扱う
    if(line.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

    return cols;
}

//Git の pull などで競合したファイルを、どちらの側の内容として読むか
enum class GitConflictSide
{
    Ours,   //ローカル側。<<<<<<< から ======= まで
    Theirs, //リモート側。======= から >>>>>>> まで
};

//Git の競合マーカーの行と、side ではない側の行を取り除く。||||||| から ======= までの base の行も取り除く。
//競合マーカーの無いファイルは、そのまま返す
static QStringList pickGitConflictSide(const QStringList& lines, GitConflictSide side)
{
    const auto isMarker = [](const QString& line, const QString& marker) {
        return line == marker || line == marker + '\r' || line.startsWith(marker + ' ');
    };
    enum class Block { Outside, Ours, Base, Theirs };
    auto block = Block::Outside;
    QStringList picked;
    for(const auto& line : lines)
    {
        if(block == Block::Outside && isMarker(line, "<<<<<<<")){ block = Block::Ours; continue; }
        if(block == Block::Ours && isMarker(line, "|||||||")){ block = Block::Base; continue; }
        if((block == Block::Ours || block == Block::Base) && isMarker(line, "=======")){ block = Block::Theirs; continue; }
        if(block == Block::Theirs && isMarker(line, ">>>>>>>")){ block = Block::Outside; continue; }

        if(block == Block::Base){ continue; }
        if(block == Block::Ours && side == GitConflictSide::Theirs){ continue; }
        if(block == Block::Theirs && side == GitConflictSide::Ours){ continue; }
        picked.append(line);
    }
    return picked;
}

//ファイルの中身を CSV として読む。値はクォートを含んだまま返す。セル内の改行 \r\n / \n はファイルの内容のまま残す。
//Git の競合マーカーがあるファイルは、side の側の内容として読む
static std::vector<std::vector<QString>> parseCsv(const QByteArray& contents, GitConflictSide side = GitConflictSide::Ours)
{
    auto lines = pickGitConflictSide(QString::fromUtf8(contents).split('\n'), side);
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
            //行の区切りが \r\n のファイルの、セル外の \r
            if(line_buffer.endsWith('\r')){ line_buffer.chop(1); }
            rows.emplace_back(std::move(line_buffer));
            line_buffer = "";
        }
        else{
            line_buffer += line+'\n';	//getlineに改行は含まれないため、ここで追加する
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
        //翻訳が未入力の列など、末尾の空の列も1列として扱う
        if(reachedLineEnd == false && row.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

        csv.emplace_back(std::move(cols));
    }

    if(csv.size() < 2){ return {}; }

    return csv;
}

//Git の競合マーカーがあるファイルは、side の側の内容として読む
static std::vector<std::vector<QString>> readCsv(QString path, GitConflictSide side = GitConflictSide::Ours)
{
    //std::ifstream に toLocal8Bit() のパスを渡すと ANSI コードページ外の文字を含むパスを開けないため QFile を使う。
    //Text モードではセル内の \r も削除されるため、バイナリで読む
    QFile file(path);
    if(file.open(QIODevice::ReadOnly) == false){ return {}; }
    return parseCsv(file.readAll(), side);
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
