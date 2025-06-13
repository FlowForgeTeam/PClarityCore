#include <gtest/gtest.h>
#include "../../Process_data.h"
#include "../../Functions_file_system.h"
#include <fstream>
#include <filesystem>

class FilePersistenceTest : public ::testing::Test {
protected:
    const std::string test_file = "test_persistence.json";
    
    void SetUp() override {
        // Clean up any existing test file
        if (std::filesystem::exists(test_file)) {
            std::filesystem::remove(test_file);
        }
    }
    
    void TearDown() override {
        // Clean up test file
        if (std::filesystem::exists(test_file)) {
            std::filesystem::remove(test_file);
        }
    }
    
    Process_data CreateTestProcess(const std::string& name, int pid) {
        Win32_process_data data = {};
        data.pid = pid;
        data.exe_name = name;
        data.exe_path = "C:\\" + name;
        data.creation_time = 1000000 + pid;
        return Process_data(data);
    }
};

TEST_F(FilePersistenceTest, WriteReadJsonFile) {
    // Create test data
    std::vector<Process_data> processes;
    processes.push_back(CreateTestProcess("app1.exe", 1234));
    processes.push_back(CreateTestProcess("app2.exe", 5678));
    
    // Mark as tracked
    for (auto& proc : processes) {
        proc.is_tracked = true;
    }
    
    // Write to JSON
    json j_array = json::array();
    for (const auto& proc : processes) {
        json j_proc;
        convert_to_json(&j_proc, &proc);
        j_array.push_back(j_proc);
    }
    
    json j_root;
    j_root["processes_to_track"] = j_array;
    
    // Write to file
    std::ofstream file(test_file);
    file << j_root.dump(4);
    file.close();
    
    // Verify file exists
    ASSERT_TRUE(std::filesystem::exists(test_file));
    
    // Read back
    std::string content;
    int read_result = read_file(test_file.c_str(), &content);
    EXPECT_EQ(read_result, 0);
    EXPECT_FALSE(content.empty());
    
    // Parse JSON
    json parsed = json::parse(content);
    EXPECT_TRUE(parsed.contains("processes_to_track"));
    EXPECT_EQ(parsed["processes_to_track"].size(), 2);
}

TEST_F(FilePersistenceTest, FileReadError) {
    // Try to read non-existent file
    std::string content;
    int result = read_file("non_existent_file.json", &content);
    
    EXPECT_NE(result, 0); // Should return error
    EXPECT_TRUE(content.empty());
}

TEST_F(FilePersistenceTest, EmptyFileHandling) {
    // Create empty file
    std::ofstream file(test_file);
    file.close();
    
    // Try to read
    std::string content;
    int result = read_file(test_file.c_str(), &content);
    
    EXPECT_EQ(result, 0); // Should succeed
    EXPECT_TRUE(content.empty());
}

TEST_F(FilePersistenceTest, LargeFileHandling) {
    // Create a larger JSON file
    json j_root;
    json j_array = json::array();
    
    // Create 100 test processes
    for (int i = 0; i < 100; ++i) {
        auto proc = CreateTestProcess("app" + std::to_string(i) + ".exe", 1000 + i);
        proc.is_tracked = true;
        
        json j_proc;
        convert_to_json(&j_proc, &proc);
        j_array.push_back(j_proc);
    }
    
    j_root["processes_to_track"] = j_array;
    
    // Write large file
    std::ofstream file(test_file);
    file << j_root.dump(4);
    file.close();
    
    // Read back
    std::string content;
    int result = read_file(test_file.c_str(), &content);
    
    EXPECT_EQ(result, 0);
    EXPECT_GT(content.length(), 10000); // Should be substantial
    
    // Verify parsing
    json parsed = json::parse(content);
    EXPECT_EQ(parsed["processes_to_track"].size(), 100);
}