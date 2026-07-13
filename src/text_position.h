#pragma once
#include <QString>
#include <variant>

namespace langscore
{
struct ScriptTextPosition
{
    struct RowCol {
        size_t row = 0;
        size_t col = 0;
        bool operator==(const RowCol& other) const {
            return row == other.row && col == other.col;
        }
        bool operator==(const std::pair<size_t, size_t>& other) const {
            return row == other.first && col == other.second;
        }
    };
    struct ScriptArg {
        QString valueName;
        bool operator==(const ScriptArg& other) const {
            return valueName == other.valueName;
        }
    };
    enum class Type : int8_t {
        RowCol,
        Argument
    };

    Type type = Type::RowCol;
    bool ignore = false;
    QString originalText;
    std::variant<RowCol, ScriptArg> d;

    bool operator==(const ScriptTextPosition& other) const {
        return type == other.type
            && ignore == other.ignore
            && originalText == other.originalText
            && d == other.d;
    }

    bool operator==(const std::pair<size_t, size_t>& other) const {
        return std::get<RowCol>(d) == other;
    }
};
}