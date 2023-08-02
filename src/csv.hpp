#pragma once

#include <QString>
#include <QStringList>
#include <fstream>


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
    if(tmp.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

    return cols;
}

static std::vector<std::vector<QString>> readCsv(QString path)
{
    std::ifstream file(path.toLocal8Bit());
    if(file.bad()){ return {}; }

    std::vector<QString> rows;
    std::string _line;
    QString line_buffer;
    while(std::getline(file, _line))
    {
        QString line = QString::fromStdString(_line);
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

    bool find_dq = false;
    std::vector<std::vector<QString>> csv;
    for(auto& row : rows)
    {
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
                break;
            }
            tmp.push_back(c);
        }
        if(tmp.isEmpty() == false){ cols.emplace_back(std::move(tmp)); }

        csv.emplace_back(std::move(cols));
    }

    if(csv.size() < 2){ return {}; }

    return csv;
}

struct TextPosition
{
    struct RowCol {
        size_t row;
        size_t col;
        RowCol():row(0), col(0){}
    };
    struct ScriptArg {
        QString valueName;
        ScriptArg():valueName(""){}
    };
    enum class Type {
        RowCol,
        Argument
    };

    Type type;
    QString scriptFileName;
    std::variant<RowCol, ScriptArg> d;
};

static TextPosition parseScriptNameWithRowCol(QString script)
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

    TextPosition result;
    if(rowOk && colOk){
        //fileName:row:colの形式でパースできた場合の処理
        result.scriptFileName = script.remove(rowStart, script.size()-rowStart);
        auto cell = TextPosition::RowCol{};
        cell.col = col; cell.row = row;
        result.d = cell;
    }
    else{
        //それ以外はfileName:変数名として認識する。
        //既に文字列を分割しているので、該当する変数を代入。
        result.type = TextPosition::Type::Argument;
        result.scriptFileName = rowStr;
        auto cell = TextPosition::ScriptArg{};
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
    return std::forward_as_tuple(list[0].toUInt(), list[1].toUInt());
}

}
