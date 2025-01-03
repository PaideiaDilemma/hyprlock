#include "MiniJsonParser.hpp"
#include <iostream>

int _main() {
    const std::string in = R"({"type": "asdf", "array": ["a", "b", "c"]})";

    auto [result, error] = MiniJsonParse::parse(in);
    if (error.status != SMiniJsonError::MINI_JSON_OK) {
        std::cerr << "Error: " << error.message << std::endl;
        return 1;
    }

    for (const auto& [key, value] : result.values) {
        if (std::holds_alternative<std::string_view>(value))
            std::cout << key << ": " << std::get<std::string_view>(value) << std::endl;
        else {
            std::cout << key << ": ";
            for (const auto& item : std::get<std::vector<std::string_view>>(value)) {
                std::cout << item << " ";
            }
            std::cout << std::endl;
        }
    }

    std::cout << MiniJsonSerialize::serialize(result) << std::endl;
    return 0;
}
