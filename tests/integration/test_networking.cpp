#include <gtest/gtest.h>
#include "../../Commands.h"
#include "../../Process_data.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>
#include <future>

#pragma comment(lib, "ws2_32.lib")

class NetworkingTest : public ::testing::Test {
protected:
    static constexpr int TEST_PORT = 12345;
    static constexpr const char* TEST_HOST = "127.0.0.1";
    
    void SetUp() override {
        // Initialize Winsock
        WSADATA wsaData;
        int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
        ASSERT_EQ(result, 0) << "WSAStartup failed";
        
        server_running = false;
        client_socket = INVALID_SOCKET;
        server_socket = INVALID_SOCKET;
    }
    
    void TearDown() override {
        // Stop server if running
        server_running = false;
        
        // Close sockets
        if (client_socket != INVALID_SOCKET) {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }
        
        if (server_socket != INVALID_SOCKET) {
            closesocket(server_socket);
            server_socket = INVALID_SOCKET;
        }
        
        // Give time for server thread to stop
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Cleanup Winsock
        WSACleanup();
    }
    
    // Simple echo server for testing
    void StartTestServer() {
        server_thread = std::thread([this]() {
            RunTestServer();
        });
        
        // Wait for server to start
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    void StopTestServer() {
        server_running = false;
        if (server_thread.joinable()) {
            server_thread.join();
        }
    }
    
    bool ConnectToTestServer() {
        client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (client_socket == INVALID_SOCKET) {
            return false;
        }
        
        sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TEST_PORT);
        inet_pton(AF_INET, TEST_HOST, &server_addr.sin_addr);
        
        int result = connect(client_socket, 
                           reinterpret_cast<sockaddr*>(&server_addr), 
                           sizeof(server_addr));
        
        return result != SOCKET_ERROR;
    }
    
    bool SendCommand(const std::string& json_command) {
        if (client_socket == INVALID_SOCKET) {
            return false;
        }
        
        int bytes_sent = send(client_socket, 
                             json_command.c_str(), 
                             static_cast<int>(json_command.length()), 
                             0);
        
        return bytes_sent != SOCKET_ERROR;
    }
    
    std::string ReceiveResponse(int timeout_ms = 5000) {
        if (client_socket == INVALID_SOCKET) {
            return "";
        }
        
        // Set receive timeout
        DWORD timeout = timeout_ms;
        setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, 
                   reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        
        char buffer[4096] = {};
        int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received > 0) {
            return std::string(buffer, bytes_received);
        }
        
        return "";
    }
    
private:
    void RunTestServer() {
        server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (server_socket == INVALID_SOCKET) {
            return;
        }
        
        // Allow socket reuse
        int reuse = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, 
                   reinterpret_cast<const char*>(&reuse), sizeof(reuse));
        
        sockaddr_in server_addr = {};
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(TEST_PORT);
        server_addr.sin_addr.s_addr = INADDR_ANY;
        
        if (bind(server_socket, 
                reinterpret_cast<sockaddr*>(&server_addr), 
                sizeof(server_addr)) == SOCKET_ERROR) {
            return;
        }
        
        if (listen(server_socket, 1) == SOCKET_ERROR) {
            return;
        }
        
        server_running = true;
        
        // Set non-blocking mode for accept
        u_long mode = 1;
        ioctlsocket(server_socket, FIONBIO, &mode);
        
        while (server_running) {
            SOCKET client = accept(server_socket, nullptr, nullptr);
            
            if (client != INVALID_SOCKET) {
                HandleClient(client);
                closesocket(client);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void HandleClient(SOCKET client) {
        char buffer[4096] = {};
        int bytes_received = recv(client, buffer, sizeof(buffer) - 1, 0);
        
        if (bytes_received > 0) {
            std::string received_data(buffer, bytes_received);
            
            // Parse as JSON command and create response
            auto command_result = command_from_json(received_data.c_str());
            
            if (command_result.second) {
                // Valid command - create appropriate response
                std::string response;
                
                switch (command_result.first.type) {
                    case Command_type::report:
                        response = R"({"status": "ok", "type": "report", "data": {"process_count": 42}})";
                        break;
                        
                    case Command_type::track:
                        response = R"({"status": "ok", "type": "track", "message": "Process tracked"})";
                        break;
                        
                    case Command_type::untrack:
                        response = R"({"status": "ok", "type": "untrack", "message": "Process untracked"})";
                        break;
                        
                    case Command_type::quit:
                        response = R"({"status": "ok", "type": "quit", "message": "Shutting down"})";
                        server_running = false;
                        break;
                        
                    case Command_type::shutdown:
                        response = R"({"status": "ok", "type": "shutdown", "message": "Server shutdown"})";
                        server_running = false;
                        break;
                        
                    default:
                        response = R"({"status": "error", "message": "Unknown command"})";
                        break;
                }
                
                send(client, response.c_str(), static_cast<int>(response.length()), 0);
            } else {
                // Invalid command
                std::string error_response = R"({"status": "error", "message": "Invalid JSON command"})";
                send(client, error_response.c_str(), static_cast<int>(error_response.length()), 0);
            }
        }
    }
    
    std::atomic<bool> server_running;
    std::thread server_thread;
    SOCKET client_socket;
    SOCKET server_socket;
};

// =============== Connection Tests ===============

TEST_F(NetworkingTest, CanStartAndStopServer) {
    ASSERT_NO_THROW(StartTestServer());
    EXPECT_TRUE(server_running);
    
    ASSERT_NO_THROW(StopTestServer());
    EXPECT_FALSE(server_running);
}

TEST_F(NetworkingTest, CanConnectToServer) {
    StartTestServer();
    
    bool connected = ConnectToTestServer();
    EXPECT_TRUE(connected);
    
    StopTestServer();
}

TEST_F(NetworkingTest, ConnectionFailsWhenServerNotRunning) {
    // Don't start server
    bool connected = ConnectToTestServer();
    EXPECT_FALSE(connected);
}

// =============== Command Communication Tests ===============

TEST_F(NetworkingTest, SendReportCommand) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    std::string command = R"({"command_id": 0, "extra": {}})";
    bool sent = SendCommand(command);
    EXPECT_TRUE(sent);
    
    std::string response = ReceiveResponse();
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(response.find("report") != std::string::npos);
    EXPECT_TRUE(response.find("ok") != std::string::npos);
    
    StopTestServer();
}

TEST_F(NetworkingTest, SendTrackCommand) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    std::string command = R"({"command_id": 3, "extra": {"path": "notepad.exe"}})";
    bool sent = SendCommand(command);
    EXPECT_TRUE(sent);
    
    std::string response = ReceiveResponse();
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(response.find("track") != std::string::npos);
    EXPECT_TRUE(response.find("ok") != std::string::npos);
    
    StopTestServer();
}

TEST_F(NetworkingTest, SendUntrackCommand) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    std::string command = R"({"command_id": 4, "extra": {"path": "notepad.exe"}})";
    bool sent = SendCommand(command);
    EXPECT_TRUE(sent);
    
    std::string response = ReceiveResponse();
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(response.find("untrack") != std::string::npos);
    EXPECT_TRUE(response.find("ok") != std::string::npos);
    
    StopTestServer();
}

TEST_F(NetworkingTest, SendInvalidCommand) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    std::string command = "invalid json";
    bool sent = SendCommand(command);
    EXPECT_TRUE(sent);
    
    std::string response = ReceiveResponse();
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(response.find("error") != std::string::npos);
    
    StopTestServer();
}

TEST_F(NetworkingTest, SendQuitCommand) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    std::string command = R"({"command_id": 1, "extra": {}})";
    bool sent = SendCommand(command);
    EXPECT_TRUE(sent);
    
    std::string response = ReceiveResponse();
    EXPECT_FALSE(response.empty());
    EXPECT_TRUE(response.find("quit") != std::string::npos);
    
    // Server should stop after quit command
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(server_running);
    
    StopTestServer();
}

// =============== Multiple Client Tests ===============

TEST_F(NetworkingTest, MultipleSequentialConnections) {
    StartTestServer();
    
    // First connection
    {
        ASSERT_TRUE(ConnectToTestServer());
        
        std::string command = R"({"command_id": 0, "extra": {}})";
        EXPECT_TRUE(SendCommand(command));
        
        std::string response = ReceiveResponse();
        EXPECT_FALSE(response.empty());
        
        closesocket(client_socket);
        client_socket = INVALID_SOCKET;
    }
    
    // Second connection
    {
        ASSERT_TRUE(ConnectToTestServer());
        
        std::string command = R"({"command_id": 3, "extra": {"path": "test.exe"}})";
        EXPECT_TRUE(SendCommand(command));
        
        std::string response = ReceiveResponse();
        EXPECT_FALSE(response.empty());
        EXPECT_TRUE(response.find("track") != std::string::npos);
    }
    
    StopTestServer();
}

// =============== Error Handling Tests ===============

TEST_F(NetworkingTest, HandleConnectionTimeout) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    // Send command but don't expect response (simulate timeout)
    std::string command = R"({"command_id": 0, "extra": {}})";
    EXPECT_TRUE(SendCommand(command));
    
    // Try to receive with very short timeout
    std::string response = ReceiveResponse(100); // 100ms timeout
    // Response might be empty due to timeout, but that's ok for this test
    
    StopTestServer();
}

TEST_F(NetworkingTest, HandleLargeCommand) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    // Create a large command with long path
    std::string long_path = "C:\\";
    for (int i = 0; i < 100; ++i) {
        long_path += "VeryLongDirectoryName\\";
    }
    long_path += "app.exe";
    
    std::string command = R"({"command_id": 3, "extra": {"path": ")" + long_path + R"("}})";
    bool sent = SendCommand(command);
    EXPECT_TRUE(sent);
    
    std::string response = ReceiveResponse();
    // Server should handle large commands gracefully
    EXPECT_FALSE(response.empty());
    
    StopTestServer();
}

// =============== Protocol Validation Tests ===============

TEST_F(NetworkingTest, CommandResponseProtocol) {
    StartTestServer();
    ASSERT_TRUE(ConnectToTestServer());
    
    struct TestCase {
        std::string command;
        std::string expected_type;
        std::string expected_status;
    };
    
    std::vector<TestCase> test_cases = {
        {R"({"command_id": 0, "extra": {}})", "report", "ok"},
        {R"({"command_id": 3, "extra": {"path": "test.exe"}})", "track", "ok"},
        {R"({"command_id": 4, "extra": {"path": "test.exe"}})", "untrack", "ok"},
        {"invalid", "", "error"}
    };
    
    for (const auto& test_case : test_cases) {
        // Reconnect for each test
        if (client_socket != INVALID_SOCKET) {
            closesocket(client_socket);
            client_socket = INVALID_SOCKET;
        }
        
        ASSERT_TRUE(ConnectToTestServer()) << "Failed to connect for test case: " << test_case.command;
        
        EXPECT_TRUE(SendCommand(test_case.command));
        std::string response = ReceiveResponse();
        
        EXPECT_FALSE(response.empty()) << "Empty response for: " << test_case.command;
        EXPECT_TRUE(response.find(test_case.expected_status) != std::string::npos) 
            << "Expected status '" << test_case.expected_status << "' not found in: " << response;
        
        if (!test_case.expected_type.empty()) {
            EXPECT_TRUE(response.find(test_case.expected_type) != std::string::npos)
                << "Expected type '" << test_case.expected_type << "' not found in: " << response;
        }
    }
    
    StopTestServer();
}

// =============== Performance Tests ===============

TEST_F(NetworkingTest, ConcurrentCommandPerformance) {
    StartTestServer();
    
    const int num_commands = 10;
    std::vector<std::future<bool>> futures;
    
    // Launch concurrent clients
    for (int i = 0; i < num_commands; ++i) {
        futures.push_back(std::async(std::launch::async, [this, i]() {
            SOCKET test_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (test_socket == INVALID_SOCKET) {
                return false;
            }
            
            sockaddr_in server_addr = {};
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(TEST_PORT);
            inet_pton(AF_INET, TEST_HOST, &server_addr.sin_addr);
            
            if (connect(test_socket, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) == SOCKET_ERROR) {
                closesocket(test_socket);
                return false;
            }
            
            std::string command = R"({"command_id": 0, "extra": {}})";
            int bytes_sent = send(test_socket, command.c_str(), static_cast<int>(command.length()), 0);
            
            if (bytes_sent != SOCKET_ERROR) {
                char buffer[1024] = {};
                recv(test_socket, buffer, sizeof(buffer) - 1, 0);
            }
            
            closesocket(test_socket);
            return bytes_sent != SOCKET_ERROR;
        }));
    }
    
    // Wait for all to complete
    int successful = 0;
    for (auto& future : futures) {
        if (future.get()) {
            successful++;
        }
    }
    
    // Should handle most concurrent connections
    EXPECT_GE(successful, num_commands / 2);
    
    StopTestServer();
}