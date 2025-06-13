#include "gtest/gtest.h"
#include "Functions_win32.h" // Include the file containing wchar_to_utf8

// Test with nullptr input
TEST(WCharToUtf8Test, HandlesNullInput) {
    std::string result = wchar_to_utf8(nullptr);
    EXPECT_EQ("", result);
}

// Test with empty string
TEST(WCharToUtf8Test, HandlesEmptyString) {
    std::string result = wchar_to_utf8(L"");
    EXPECT_EQ("", result);
}

// Test with basic ASCII characters
TEST(WCharToUtf8Test, HandlesBasicAscii) {
    std::string result = wchar_to_utf8(L"Hello World");
    EXPECT_EQ("Hello World", result);
}

// Test with mixed ASCII and non-ASCII characters
TEST(WCharToUtf8Test, HandlesMixedCharacters) {
    std::string result = wchar_to_utf8(L"Hello $¢");
    // UTF-8 representation of "Hello ??"
    const char expected[] = "Hello $\xc2\xa2";
    EXPECT_EQ(expected, result);
}

// Test with special characters
TEST(WCharToUtf8Test, HandlesSpecialCharacters) {
    std::string result = wchar_to_utf8(L"€£¥$");
    // UTF-8 representation of "€£¥$¢"
    const char expected[] = "\xe2\x82\xac\xc2\xa3\xc2\xa5$\xc2\xa2";
    EXPECT_EQ(expected, result);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}