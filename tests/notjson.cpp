#include <print>
#include <variant>
#include "shared.hpp"
#include "../src/helpers/NotJson.hpp"

int main() {
    const std::string in  = R"({"type":"asdf","array":["a","b","c"]})";
    int               ret = 0;

    auto [result, error] = NNotJson::parse(in);
    EXPECT(error.status, NNotJson::SError::NOT_JSON_OK);

    EXPECT(result.values.size(), 2);
    EXPECT(std::holds_alternative<std::string>(result.values["type"]), true);
    EXPECT(std::get<std::string>(result.values["type"]), "asdf");

    EXPECT(std::holds_alternative<std::vector<std::string>>(result.values["array"]), true);
    const auto vec = std::get<std::vector<std::string>>(result.values["array"]);
    EXPECT(vec.size(), 3);
    EXPECT(vec[0], std::string{"a"});
    EXPECT(vec[1], std::string{"b"});
    EXPECT(vec[2], std::string{"c"});

    const auto serialized = NNotJson::serialize(result);

    std::print("serialized: {}\n", serialized);
    // order is not guaranteed
    EXPECT(serialized == in || serialized == R"({"array":["a","b","c"],"type":"asdf"})", true);

    const auto in2         = R"({"type":"auth_message","auth_message_type":"secret","auth_message":"Password: "})";
    auto [result2, error2] = NNotJson::parse(in2);

    EXPECT(error2.status, NNotJson::SError::NOT_JSON_OK);
    EXPECT(result2.values.size(), 3);
    EXPECT(std::holds_alternative<std::string>(result2.values["type"]), true);
    EXPECT(std::get<std::string>(result2.values["type"]), "auth_message");

    return ret;
}
