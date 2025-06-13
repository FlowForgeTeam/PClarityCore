#include <gtest/gtest.h>
#include <iostream>

int main(int argc, char** argv) {
    std::cout << "🧪 Starting PClarity Test Suite" << std::endl;
    std::cout << "=================================" << std::endl;
    
    ::testing::InitGoogleTest(&argc, argv);
    
    // Configure test output
    ::testing::FLAGS_gtest_output = "xml:test_results.xml";
    ::testing::FLAGS_gtest_print_time = true;
    
    std::cout << "Running tests..." << std::endl;
    int result = RUN_ALL_TESTS();
    
    if (result == 0) {
        std::cout << "✅ All tests passed!" << std::endl;
    } else {
        std::cout << "❌ Some tests failed!" << std::endl;
    }
    
    return result;
}