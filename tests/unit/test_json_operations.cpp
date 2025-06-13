#include <gtest/gtest.h>
#include "../../Process_data.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class JsonOperationsTest : public ::testing::Test {
protected:
    Process_data CreateTestProcess() {
        Win32_process_data data = {};
        data.pid = 1234;
        data.exe_name = "test.exe";
        data.exe_path = "C:\\test.exe";
        data.creation_time = 1000000;
        data.priority_class = 32;
        
        return Process_data(data);
    }
};

// =============== JSON Serialization Tests ===============

TEST_F(JsonOperationsTest, ConvertToJson_BasicProcess) {
    auto process = CreateTestProcess();
    json j;
    
    convert_to_json(&j, &process);
    
    EXPECT_TRUE(j.contains("data"));
    EXPECT_TRUE(j.contains("is_active"));
    EXPECT_TRUE(j.contains("is_tracked"));
    EXPECT_TRUE(j.contains("sessions"));
    
    EXPECT_EQ(j["data"]["pid"], 1234);
    EXPECT_EQ(j["data"]["exe_name"], "test.exe");
    EXPECT_EQ(j["data"]["exe_path"], "C:\\test.exe");
    EXPECT_FALSE(j["is_active"]);
    EXPECT_FALSE(j["is_tracked"]);
}

TEST_F(JsonOperationsTest, ConvertToJson_WithSessions) {
    auto process = CreateTestProcess();
    process.is_tracked = true;
    
    // Add a session by activating and deactivating
    process.update_active();
    process.update_inactive();
    
    json j;
    convert_to_json(&j, &process);
    
    EXPECT_TRUE(j.contains("sessions"));
    EXPECT_EQ(j["sessions"].size(), 1);
    EXPECT_TRUE(j["sessions"][0].contains("start_time"));
    EXPECT_TRUE(j["sessions"][0].contains("end_time"));
}

// =============== JSON Deserialization Tests ===============

TEST_F(JsonOperationsTest, ConvertFromJson_BasicProcess) {
    // Create JSON manually
    json j = {
        {"start", 1000000},
        {"is_active", true},
        {"is_tracked", false},
        {"was_updated", true},
        {"sessions", json::array()},
        {"data", {
            {"pid", 5678},
            {"started_threads", 3},
            {"ppid", 1000},
            {"base_priority", 8},
            {"exe_name", "parsed.exe"},
            {"exe_path", "C:\\parsed.exe"},
            {"priority_class", 64}
        }}
    };
    
    Win32_process_data dummy_data = {};
    Process_data process(dummy_data);
    
    int result = convert_from_json(&j, &process);
    
    EXPECT_EQ(result, 0); // Success
    EXPECT_EQ(process.data.pid, 5678);
    EXPECT_EQ(process.data.exe_name, "parsed.exe");
    EXPECT_EQ(process.data.exe_path, "C:\\parsed.exe");
    EXPECT_TRUE(process.is_active);
    EXPECT_FALSE(process.is_tracked);
    EXPECT_TRUE(process.was_updated);
}

TEST_F(JsonOperationsTest, ConvertFromJson_WithSessions) {
    json j = {
        {"start", 1000000},
        {"is_active", false},
        {"is_tracked", true},
        {"was_updated", false},
        {"sessions", json::array({
            {{"start_time", 2000000}, {"end_time", 3000000}},
            {{"start_time", 4000000}, {"end_time", 5000000}}
        })},
        {"data", {
            {"pid", 9999},
            {"started_threads", 5},
            {"ppid", 8888},
            {"base_priority", 10},
            {"exe_name", "session_test.exe"},
            {"exe_path", "C:\\session_test.exe"},
            {"priority_class", 128}
        }}
    };
    
    Win32_process_data dummy_data = {};
    Process_data process(dummy_data);
    
    int result = convert_from_json(&j, &process);
    
    EXPECT_EQ(result, 0); // Success
    EXPECT_EQ(process.sessions.size(), 2);
    EXPECT_TRUE(process.is_tracked);
}

TEST_F(JsonOperationsTest, ConvertFromJson_InvalidJson) {
    json j = {
        {"invalid_field", "value"}
        // Missing required fields
    };
    
    Win32_process_data dummy_data = {};
    Process_data process(dummy_data);
    
    int result = convert_from_json(&j, &process);
    
    EXPECT_EQ(result, -1); // Error
}

// =============== Round-trip Tests ===============

TEST_F(JsonOperationsTest, RoundTrip_PreservesData) {
    auto original = CreateTestProcess();
    original.is_tracked = true;
    original.is_active = true;
    
    // Add sessions
    original.update_active();
    original.update_inactive();
    
    // Convert to JSON
    json j;
    convert_to_json(&j, &original);
    
    // Convert back to Process_data
    Win32_process_data dummy_data = {};
    Process_data restored(dummy_data);
    int result = convert_from_json(&j, &restored);
    
    EXPECT_EQ(result, 0);
    EXPECT_EQ(restored.data.pid, original.data.pid);
    EXPECT_EQ(restored.data.exe_name, original.data.exe_name);
    EXPECT_EQ(restored.data.exe_path, original.data.exe_path);
    EXPECT_EQ(restored.sessions.size(), original.sessions.size());
}