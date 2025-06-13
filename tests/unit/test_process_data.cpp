#include <gtest/gtest.h>
#include "../../Process_data.h"
#include <thread>
#include <chrono>

class ProcessDataTest : public ::testing::Test {
protected:
    Win32_process_data CreateTestProcessData() {
        Win32_process_data data = {};
        data.pid = 1234;
        data.ppid = 567;
        data.exe_name = "test.exe";
        data.exe_path = "C:\\test\\test.exe";
        data.creation_time = 1000000;
        data.priority_class = 32;
        data.started_threads = 5;
        data.ram_usage = 1024 * 1024; // 1MB
        return data;
    }
};

// =============== Constructor Tests ===============

TEST_F(ProcessDataTest, Constructor_InitializesCorrectly) {
    auto win32_data = CreateTestProcessData();
    Process_data process(win32_data);
    
    // Check data is copied correctly
    EXPECT_EQ(process.data.pid, 1234);
    EXPECT_EQ(process.data.exe_name, "test.exe");
    EXPECT_EQ(process.data.exe_path, "C:\\test\\test.exe");
    EXPECT_EQ(process.data.creation_time, 1000000);
    
    // Check initial state
    EXPECT_FALSE(process.is_active);
    EXPECT_FALSE(process.is_tracked);
    EXPECT_FALSE(process.was_updated);
    EXPECT_TRUE(process.sessions.empty());
}

// =============== State Management Tests ===============

TEST_F(ProcessDataTest, UpdateActive_FirstTime) {
    auto win32_data = CreateTestProcessData();
    Process_data process(win32_data);
    
    auto start_time = process.start;
    
    process.update_active();
    
    EXPECT_TRUE(process.is_active);
    EXPECT_TRUE(process.was_updated);
    // Start time should be updated
    EXPECT_GE(process.start, start_time);
}

TEST_F(ProcessDataTest, UpdateActive_AlreadyActive) {
    auto win32_data = CreateTestProcessData();
    Process_data process(win32_data);
    
    // First activation
    process.update_active();
    auto first_start = process.start;
    process.was_updated = false;
    
    // Second activation (should not change start time)
    process.update_active();
    
    EXPECT_TRUE(process.is_active);
    EXPECT_TRUE(process.was_updated);
    EXPECT_EQ(process.start, first_start); // Start time unchanged
}

TEST_F(ProcessDataTest, UpdateInactive_FromActive) {
    auto win32_data = CreateTestProcessData();
    Process_data process(win32_data);
    
    // Make it tracked so session gets recorded
    process.is_tracked = true;
    
    // Activate then deactivate
    process.update_active();
    process.was_updated = false;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Small delay
    
    process.update_inactive();
    
    EXPECT_FALSE(process.is_active);
    EXPECT_TRUE(process.was_updated);
    EXPECT_EQ(process.sessions.size(), 1); // Session should be recorded
}

TEST_F(ProcessDataTest, UpdateInactive_NotTracked) {
    auto win32_data = CreateTestProcessData();
    Process_data process(win32_data);
    
    // Not tracked, so no session should be recorded
    process.update_active();
    process.was_updated = false;
    process.update_inactive();
    
    EXPECT_FALSE(process.is_active);
    EXPECT_TRUE(process.was_updated);
    EXPECT_TRUE(process.sessions.empty()); // No session recorded
}

// =============== Equality Tests ===============

TEST_F(ProcessDataTest, Equality_SameProcess) {
    auto data1 = CreateTestProcessData();
    auto data2 = CreateTestProcessData();
    
    Process_data process1(data1);
    Process_data process2(data2);
    
    EXPECT_TRUE(process1 == process2);
    EXPECT_TRUE(process1 == data2);
}

TEST_F(ProcessDataTest, Equality_DifferentPath) {
    auto data1 = CreateTestProcessData();
    auto data2 = CreateTestProcessData();
    data2.exe_path = "C:\\different\\path.exe";
    
    Process_data process1(data1);
    Process_data process2(data2);
    
    EXPECT_FALSE(process1 == process2);
}

TEST_F(ProcessDataTest, Equality_DifferentCreationTime) {
    auto data1 = CreateTestProcessData();
    auto data2 = CreateTestProcessData();
    data2.creation_time = 2000000;
    
    Process_data process1(data1);
    Process_data process2(data2);
    
    EXPECT_FALSE(process1 == process2);
}

// =============== Session Tests ===============

TEST_F(ProcessDataTest, Session_Constructor) {
    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(10);
    
    Process_data::Session session(start, end);
    
    EXPECT_EQ(session.start_time, start);
    EXPECT_EQ(session.end_time, end);
}

TEST_F(ProcessDataTest, MultipleActivationCycles) {
    auto win32_data = CreateTestProcessData();
    Process_data process(win32_data);
    process.is_tracked = true;
    
    // Multiple activation/deactivation cycles
    for (int i = 0; i < 3; ++i) {
        process.update_active();
        process.was_updated = false;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        
        process.update_inactive();
        process.was_updated = false;
    }
    
    EXPECT_EQ(process.sessions.size(), 3);
    EXPECT_FALSE(process.is_active);
}