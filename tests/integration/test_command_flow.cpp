#include <gtest/gtest.h>
#include "../../Commands.h"
#include "../../Global_state.h"
#include <fstream>

class CommandFlowTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create clean test environment
        if (std::filesystem::exists("test_data.json")) {
            std::filesystem::remove("test_data.json");
        }
    }
    
    void TearDown() override {
        // Cleanup
        if (std::filesystem::exists("test_data.json")) {
            std::filesystem::remove("test_data.json");
        }
    }
};

TEST_F(CommandFlowTest, CompleteTrackUntrackFlow) {
    // Test complete flow: parse track command -> execute -> parse untrack -> execute
    
    // 1. Parse track command
    const char* track_json = R"({"command_id": 3, "extra": {"path": "C:\\Windows\\notepad.exe"}})";
    auto track_cmd = command_from_json(track_json);
    
    ASSERT_TRUE(track_cmd.second);
    EXPECT_EQ(track_cmd.first.type, Command_type::track);
    
    // 2. Execute track (simulate)
    std::string path = track_cmd.first.data.track.path;
    EXPECT_EQ(path, "C:\\Windows\\notepad.exe");
    
    // 3. Parse untrack command
    const char* untrack_json = R"({"command_id": 4, "extra": {"path": "C:\\Windows\\notepad.exe"}})";
    auto untrack_cmd = command_from_json(untrack_json);
    
    ASSERT_TRUE(untrack_cmd.second);
    EXPECT_EQ(untrack_cmd.first.type, Command_type::untrack);
    EXPECT_EQ(untrack_cmd.first.data.untrack.path, path);
}

TEST_F(CommandFlowTest, AllCommandTypesFlow) {
    // Test that all command types can be parsed in sequence
    
    std::vector<std::pair<const char*, Command_type>> test_commands = {
        {R"({"command_id": 0, "extra": {}})", Command_type::report},
        {R"({"command_id": 1, "extra": {}})", Command_type::quit},
        {R"({"command_id": 2, "extra": {}})", Command_type::shutdown},
        {R"({"command_id": 3, "extra": {"path": "app.exe"}})", Command_type::track},
        {R"({"command_id": 4, "extra": {"path": "app.exe"}})", Command_type::untrack}
    };
    
    for (const auto& [json_str, expected_type] : test_commands) {
        auto result = command_from_json(json_str);
        ASSERT_TRUE(result.second) << "Failed to parse: " << json_str;
        EXPECT_EQ(result.first.type, expected_type);
    }
}