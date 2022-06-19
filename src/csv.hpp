#pragma once

#include <QString>
#include <fstream>


namespace langscore
{

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

static std::tuple<QString, size_t, size_t> parseScriptNameWithRowCol(QString script)
{
    auto colStart = script.lastIndexOf(":");
    auto col = script.mid(colStart+1, script.size()-colStart).toUInt();
    script.remove(colStart, script.size()-colStart);
    auto rowStart = script.lastIndexOf(":");
    auto row = script.mid(rowStart+1, colStart-rowStart).toUInt();

    auto fileName = script.remove(rowStart, script.size()-rowStart);

    return std::forward_as_tuple(fileName, row, col);
}

}
