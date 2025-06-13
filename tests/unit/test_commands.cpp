#include <gtest/gtest.h>
#include "../../Commands.h"

class CommandsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup for each test
    }
    
    void TearDown() override {
        // Cleanup after each test  
    }
};

// =============== Command Type Tests ===============

TEST_F(CommandsTest, CommandTypeFromInt_AllValidTypes) {
    // Test all valid command types
    for (int i = 0; i <= 4; ++i) {
        auto result = command_type_from_int(i);
        EXPECT_TRUE(result.second) << "Command ID " << i << " should be valid";
    }
}

TEST_F(CommandsTest, CommandTypeFromInt_InvalidTypes) {
    std::vector<int> invalid_ids = {-1, 5, 10, 999, -999};
    
    for (int id : invalid_ids) {
        auto result = command_type_from_int(id);
        EXPECT_FALSE(result.second) << "Command ID " << id << " should be invalid";
        EXPECT_EQ(result.first, Command_type::report) << "Should default to report";
    }
}

// =============== JSON Parsing Tests ===============

TEST_F(CommandsTest, CommandFromJson_ValidReportCommand) {
    const char* json_str = R"({"command_id": 0, "extra": {}})";
    auto result = command_from_json(json_str);
    
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first.type, Command_type::report);
}

TEST_F(CommandsTest, CommandFromJson_ValidQuitCommand) {
    const char* json_str = R"({"command_id": 1, "extra": {}})";
    auto result = command_from_json(json_str);
    
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first.type, Command_type::quit);
}

TEST_F(CommandsTest, CommandFromJson_ValidTrackCommand) {
    const char* json_str = R"({"command_id": 3, "extra": {"path": "C:\\test\\app.exe"}})";
    auto result = command_from_json(json_str);
    
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first.type, Command_type::track);
    EXPECT_EQ(result.first.data.track.path, "C:\\test\\app.exe");
}

TEST_F(CommandsTest, CommandFromJson_ValidUntrackCommand) {
    const char* json_str = R"({"command_id": 4, "extra": {"path": "notepad.exe"}})";
    auto result = command_from_json(json_str);
    
    ASSERT_TRUE(result.second);
    EXPECT_EQ(result.first.type, Command_type::untrack);
    EXPECT_EQ(result.first.data.untrack.path, "notepad.exe");
}

TEST_F(CommandsTest, CommandFromJson_MissingPath) {
    const char* json_str = R"({"command_id": 3, "extra": {}})";
    auto result = command_from_json(json_str);
    
    EXPECT_FALSE(result.second);
}

TEST_F(CommandsTest, CommandFromJson_InvalidJson) {
    std::vector<const char*> invalid_jsons = {
        "invalid json",
        "{",
        "{}",
        R"({"command_id": "not_a_number"})",
        R"({"wrong_field": 0})",
        ""
    };
    
    for (const char* json_str : invalid_jsons) {
        auto result = command_from_json(json_str);
        EXPECT_FALSE(result.second) << "Should reject: " << json_str;
    }
}

// =============== Command Construction Tests ===============

TEST_F(CommandsTest, CommandDefaultConstructor) {
    Command cmd;
    EXPECT_EQ(cmd.type, Command_type::report);
}

TEST_F(CommandsTest, CommandCopyConstructor) {
    // Create original command
    const char* json_str = R"({"command_id": 3, "extra": {"path": "test.exe"}})";
    auto original = command_from_json(json_str);
    ASSERT_TRUE(original.second);
    
    // Copy construct
    Command copy(original.first);
    
    EXPECT_EQ(copy.type, Command_type::track);
    EXPECT_EQ(copy.data.track.path, "test.exe");
}

TEST_F(CommandsTest, CommandAssignmentOperator) {
    // Create commands
    const char* json1 = R"({"command_id": 3, "extra": {"path": "app1.exe"}})";
    const char* json2 = R"({"command_id": 4, "extra": {"path": "app2.exe"}})";
    
    auto cmd1 = command_from_json(json1);
    auto cmd2 = command_from_json(json2);
    
    ASSERT_TRUE(cmd1.second && cmd2.second);
    
    // Test assignment
    cmd1.first = cmd2.first;
    
    EXPECT_EQ(cmd1.first.type, Command_type::untrack);
    EXPECT_EQ(cmd1.first.data.untrack.path, "app2.exe");
}