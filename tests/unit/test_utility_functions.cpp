#include <gtest/gtest.h>
#include "../../Functions_win32.h"

class UtilityFunctionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// =============== String Conversion Tests ===============

TEST_F(UtilityFunctionsTest, WcharToUtf8_SimpleAscii) {
    const wchar_t* input = L"Hello World";
    std::string result = wchar_to_utf8(input);
    EXPECT_EQ(result, "Hello World");
}

TEST_F(UtilityFunctionsTest, WcharToUtf8_EmptyString) {
    const wchar_t* input = L"";
    std::string result = wchar_to_utf8(input);
    EXPECT_EQ(result, "");
}

TEST_F(UtilityFunctionsTest, WcharToUtf8_NullPointer) {
    std::string result = wchar_to_utf8(nullptr);
    EXPECT_EQ(result, "");
}

TEST_F(UtilityFunctionsTest, WcharToUtf8_SpecialCharacters) {
    const wchar_t* input = L"Test!@#$%^&*()";
    std::string result = wchar_to_utf8(input);
    EXPECT_EQ(result, "Test!@#$%^&*()");
}

TEST_F(UtilityFunctionsTest, WcharToUtf8_Numbers) {
    const wchar_t* input = L"1234567890";
    std::string result = wchar_to_utf8(input);
    EXPECT_EQ(result, "1234567890");
}

TEST_F(UtilityFunctionsTest, WcharToUtf8_WindowsPaths) {
    const wchar_t* input = L"C:\\Program Files\\App\\app.exe";
    std::string result = wchar_to_utf8(input);
    EXPECT_EQ(result, "C:\\Program Files\\App\\app.exe");
}

TEST_F(UtilityFunctionsTest, WcharToUtf8_LongString) {
    // Test with a longer string
    std::wstring long_input = L"This is a very long string that contains many characters ";
    for (int i = 0; i < 10; ++i) {
        long_input += L"and more content ";
    }
    
    std::string result = wchar_to_utf8(long_input.c_str());
    EXPECT_GT(result.length(), 100);
    EXPECT_TRUE(result.find("This is a very long string") == 0);
}

// =============== Win32 Error Enum Tests ===============

TEST_F(UtilityFunctionsTest, Win32ErrorEnumValues) {
    // Test that enum values are as expected
    EXPECT_EQ(static_cast<int>(Win32_error::ok), 0);
    
    // Test that different errors have different values
    EXPECT_NE(Win32_error::ok, Win32_error::CreateToolhelp32Snapshot_failed);
    EXPECT_NE(Win32_error::ok, Win32_error::Process32First_failed);
}

// =============== Data Structure Tests ===============

TEST_F(UtilityFunctionsTest, Win32ProcessData_Initialization) {
    Win32_process_data data = {};
    
    // Check zero initialization
    EXPECT_EQ(data.pid, 0);
    EXPECT_EQ(data.ppid, 0);
    EXPECT_EQ(data.started_threads, 0);
    EXPECT_EQ(data.base_priority, 0);
    EXPECT_TRUE(data.exe_name.empty());
    EXPECT_TRUE(data.exe_path.empty());
}

TEST_F(UtilityFunctionsTest, Win32ProcessData_Assignment) {
    Win32_process_data data;
    data.pid = 1234;
    data.exe_name = "test.exe";
    data.exe_path = "C:\\test.exe";
    data.creation_time = 123456789;
    data.ram_usage = 1024 * 1024;
    
    EXPECT_EQ(data.pid, 1234);
    EXPECT_EQ(data.exe_name, "test.exe");
    EXPECT_EQ(data.exe_path, "C:\\test.exe");
    EXPECT_EQ(data.creation_time, 123456789);
    EXPECT_EQ(data.ram_usage, 1024 * 1024);
}