#include "./src/helpers/SensitiveString.hpp"
#include "shared.hpp"

#define EXPECT_EMPTY(s)                                                                                                                                                            \
    EXPECT(s.empty(), true);                                                                                                                                                       \
    EXPECT(s.length(), 0);                                                                                                                                                         \
    EXPECT(strlen(s.c_str()), 0);

#define EXPECT_EQUAL_TO_REF(s, ref)                                                                                                                                                \
    EXPECT(strcmp(s.c_str(), ref.c_str()), 0);                                                                                                                                     \
    EXPECT(s.length(), ref.length());                                                                                                                                              \
    EXPECT(s.length(), strlen(s.c_str()));

int main() {
    int ret = 0;

    // Test empty
    SensitiveString empty;
    EXPECT_EMPTY(empty);

    // Test freeing
    {
        SensitiveString a, b, c, d;
        a.set("a");
        b.set("b");
        c.set("c");
        d.set("d");

        EXPECT(a.back(), 'a');
        EXPECT(b.back(), 'b');
        EXPECT(c.back(), 'c');
        EXPECT(d.back(), 'd');
    }

    // Test SensitiveString(const std::string &
    std::string     ref = "Hello, World!";
    SensitiveString s(ref.c_str());
    EXPECT_EQUAL_TO_REF(s, ref);

    // Test extending
    std::string append = " This is a test!";
    ref += append;

    s.extend(append.data(), append.size());
    EXPECT_EQUAL_TO_REF(s, ref);

    // Test clearing
    s.clear();
    EXPECT_EMPTY(s);

    // Test setting
    s.set(ref.c_str());
    EXPECT_EQUAL_TO_REF(s, ref);

    // Test pop-back
    while (!ref.empty()) {
        s.pop_back();
        ref.pop_back();
        EXPECT(s.back(), ref.back());
    }

    // Test maximium size
    ref = std::string(SensitiveString::FIXED_BUFFER_SIZE - 1, 'A');
    s.set(ref.c_str());

    EXPECT_EQUAL_TO_REF(s, ref);

    // Test explicit copy
    SensitiveString cp;
    cp.set(s);

    EXPECT_EQUAL_TO_REF(cp, ref);

    // Test extending to maximum size
    s.clear();
    ref = std::string(SensitiveString::FIXED_BUFFER_SIZE - 10, 'A');
    s.set(ref.c_str());

    append = std::string(9, 'B');
    ref += append;

    s.extend(append.data(), append.size());

    EXPECT_EQUAL_TO_REF(s, ref);

    // Test extending beyond maximum size
    s.extend(append.data(), 1);

    EXPECT_EMPTY(s);

    // Test constructing beyond maximum size
    ref = std::string(SensitiveString::FIXED_BUFFER_SIZE, 'A');
    SensitiveString tooLarge(ref.c_str());

    EXPECT_EMPTY(tooLarge);
    return ret;
}
