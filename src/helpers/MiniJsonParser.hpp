#pragma once

/*
  This is a mini json parser intended to parse the greetd protocol.
  It makes the following assumptions:
  - Data only contains strings or vector of strings
  - Data is not nested

  For example:
  {
    "key1": "value1",
    "key2": ["value2", "value3"]
  }
*/

#include <cstdio>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>

using VJsonValue = std::variant<std::string_view, std::vector<std::string_view>>;

struct SMiniJsonObject {
    std::unordered_map<std::string_view, VJsonValue> values;
};

struct SMiniJsonError {
    enum eJsonError {
        MINI_JSON_OK,
        MINI_JSON_ERROR,
    } status = MINI_JSON_OK;

    std::string message = "";
};

namespace MiniJsonParse {
    // Attention!!!!! The input string must outlive the returned object
    inline std::pair<SMiniJsonObject, SMiniJsonError> parse(const std::string& data) {
        static constexpr const std::string sinkChars = " \t\n\r,:{}";

        SMiniJsonObject                    result{};
        std::string_view                   key{};
        std::vector<std::string_view>      array;
        size_t                             parsingArray = false;
        for (size_t i = 0; i < data.size(); i++) {
            if (sinkChars.find(data[i]) != std::string::npos)
                continue;

            switch (data[i]) {
                case '"': {
                    auto end = data.find_first_of('"', i + 1);
                    if (end == std::string::npos)
                        return {result,
                                {
                                    SMiniJsonError::MINI_JSON_ERROR,
                                    "Expected closing quote, but reached end of input",
                                }};

                    std::string_view val{data.data() + i + 1, end - (i + 1)};
                    if (key.empty())
                        key = val;
                    else if (parsingArray)
                        array.push_back(val);
                    else {
                        result.values.emplace(key, val);
                        key = std::string_view{};
                    }

                    i = end;
                } break;
                case '[': {
                    parsingArray = true;
                    if (key.empty())
                        return {result,
                                {
                                    SMiniJsonError::MINI_JSON_ERROR,
                                    "Expected key before array",
                                }};
                } break;
                case ']': {
                    result.values.emplace(key, array);
                    key          = std::string_view{};
                    parsingArray = false;
                    array.clear();
                } break;
                default:
                    if (parsingArray)
                        return {result,
                                {
                                    SMiniJsonError::MINI_JSON_ERROR,
                                    std::format("Expected closing bracked, but got \"{}\"", data[i]),
                                }};

                    return {result,
                            {
                                SMiniJsonError::MINI_JSON_ERROR,
                                std::format("Unexpected character \"{}\"", data[i]),
                            }};
            }
        };

        return {result, {}};
    }
}

namespace MiniJsonSerialize {
    inline std::string getString(const std::string_view& in) {
        return std::format("\"{}\"", in);
    }

    inline std::string getArray(const std::vector<std::string_view>& in) {
        std::stringstream result;
        result << "[";
        for (const auto& item : in) {
            result << getString(item) << ",";
        }
        result.seekp(-1, std::ios_base::end);
        result << "]";
        return result.str();
    }

    inline std::string serialize(const SMiniJsonObject& obj) {
        std::stringstream result;
        result << "{";

        for (const auto& [key, value] : obj.values) {
            result << std::format("\"{}\":", key);
            if (std::holds_alternative<std::string_view>(value))
                result << getString(std::get<std::string_view>(value)) << ",";
            else
                result << getArray(std::get<std::vector<std::string_view>>(value)) << ",";
        }

        result.seekp(-1, std::ios_base::end);
        result << "}";

        return result.str();
    }
}
