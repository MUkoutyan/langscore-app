#pragma once
#include <QString>
#include <variant>

namespace langscore
{
struct TextPosition
{
    struct RowCol {
        size_t row;
        size_t col;
        RowCol() :row(0), col(0) {}
        bool operator==(const RowCol& other) const {
            return row == other.row && col == other.col;
        }
        bool operator==(const std::pair<size_t, size_t>& other) const {
            return row == other.first && col == other.second;
        }
    };
    struct ScriptArg {
        QString valueName;
        ScriptArg() :valueName("") {}
        bool operator==(const ScriptArg& other) const {
            return valueName == other.valueName;
        }
    };
    enum class Type : int8_t {
        RowCol,
        Argument
    };

    Type type;
    bool ignore = false;
    QString value;
    std::variant<RowCol, ScriptArg> d;

    bool operator==(const TextPosition& other) const {
        return type == other.type
            && ignore == other.ignore
            && value == other.value
            && d == other.d;
    }

    bool operator==(const std::pair<size_t, size_t>& other) const {
        return std::get<RowCol>(d) == other;
    }
};
}