#include "Handlers_for_main.h"
#include <fstream>

namespace Main {

    list<Command_status> command_queue; 
    bool   running         = true;
    SOCKET client_socket   = {0} ; // NOTE(damian): maybe use optional here, not sure yet.
    bool   need_new_client = true;
    string error_message   = {0} ; // NOTE(damian): maybe use optional here, not sure yet.

    void client_thread() {
        while (running) {
            if (need_new_client) {
                Main::wait_for_client_to_connect();
                need_new_client = false;
            }

            // Clearing out the command queue.
            for (auto it = command_queue.begin(); it != command_queue.end();) {
                if (it->handled)
                    it = command_queue.erase(it);
                else
                    ++it;
            }

            // TODO(damian): command_queue can can only have 2 parts. (HANDLED-UNHANDLED).
            //				 assert to make sure there is not mix up in the middle.

            // NOTE: dont forget to maybe extend it if needed, or error if the message is to long or something.
            // NOTE(damian): recv doesnt null terminate the string buffer, 
            //	             so terminating it myself, to then be able to use strcmp.

            const size_t receive_buffer_size = 512;
            char receive_buffer[receive_buffer_size];

            int n_bytes_returned = recv(client_socket, receive_buffer, receive_buffer_size, NULL); // TODO(damian): add a timeout here.
            if (n_bytes_returned == SOCKET_ERROR) {
                std::cout << "err_code: " << WSAGetLastError() << std::endl;
                closesocket(Main::client_socket);
                Main::need_new_client = true;
                continue;
            }
            if (n_bytes_returned > receive_buffer_size - 1) { // TODO(damian): handle this.
                std::cout << "bytes_returned: " << n_bytes_returned << std::endl;
                std::cout << "receive_buffer_len: " << receive_buffer_size << std::endl;
                std::cout << "flopper error" << std::endl;
                assert(false);
            }
            receive_buffer[n_bytes_returned] = '\0';

            // Overflow
            if (n_bytes_returned == receive_buffer_size) {
                // TODO: Send a respomce singaling that client has to send the same data again
            }

            //std::cout << "Received a message of " << n_bytes_returned << " bytes." << std::endl;
            //std::cout << "Message: " << "'" << receive_buffer << "'" << "\n" << std::endl;

            // Parsing the request.
            std::pair<Command, bool> result = command_from_json(receive_buffer);
            if (!result.second) {
                const char* message = "Invalid command";
                int send_err_code = send(client_socket, message, strlen(message), NULL);
                if (send_err_code == SOCKET_ERROR) {
                    // TODO: handle
                }
            }
            else {
                switch (result.first.type) {
                    case Command_type::report: {
                        Main::handle_report(&result.first.data.report);
                    } break;

                    case Command_type::quit: {
                        Main::handle_quit(&result.first.data.quit);
                    } break;

                    case Command_type::shutdown: {
                        Main::handle_shutdown(&result.first.data.shutdown);
                    } break;

                    case Command_type::track: {
                        Main::handle_track(&result.first.data.track);
                    } break;

                    case Command_type::untrack: {
                        Main::handle_untrack(&result.first.data.untrack);
                    } break;

                    case Command_type::grouped_report: {
                        assert(false);
                        //Main::handle_grouped_report(&result.first.data.grouped_report);
                    } break;

                    default: {
                        assert(false);
                    } break;
                }
            }

            // Preserve the new state
            std::ofstream data_file("data.json");

            vector<json> processes_as_jsons;
            for (Process_data& process_data : G_state::tracked_processes) {
                json temp;
                convert_to_json(&process_data, &temp);
                processes_as_jsons.push_back(temp);
            }
            json j_overall_data;
            j_overall_data["processes_to_track"] = processes_as_jsons;

            data_file << j_overall_data.dump(4) << std::endl;
            data_file.close();

            std::cout << "Saved data to file. \n" << std::endl;
        
        }
    }

    void wait_for_client_to_connect() {
        std::cout << "Waiting for a new connection with a client." << std::endl;
        pair<SOCKET, G_state::Error> result = initialise_tcp_connection_with_client();
        if (result.second.type == G_state::Error_type::ok) {
            Main::client_socket = result.first;
        }
        else assert(false);
        std::cout << "Client connected. \n" << std::endl;
    }

    void handle_report(Report_command* report) {
        std::cout << "Started reporting" << std::endl;

        vector<json> j_tracked;
        for (Process_data& data : G_state::tracked_processes) {
            json temp;
            convert_to_json(&data, &temp);
            j_tracked.push_back(temp);
        }

        vector<json> j_cur_active;
        for (Process_data& data : G_state::currently_active_processes) {
            json temp;
            convert_to_json(&data, &temp);
            j_cur_active.push_back(temp);
        }

        std::cout << "Stopped reporting" << std::endl;

        json global_json;
        global_json["tracked"]          = j_tracked;
        global_json["currently_active"] = j_cur_active;

        std::string message_as_str = global_json.dump(4);

        int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            // TODO: handle
        }

        //std::cout << "Message handling: " << message_as_str << "\n" << std::endl;
    }

    void handle_quit(Quit_command* quit) {
        string message = "Client disconnected.";
        
        int send_err_code = send(Main::client_socket, message.c_str(), message.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            // TODO: handle
        }

        closesocket(Main::client_socket);

        std::cout << "Message handling: " << message << "\n" << std::endl;

        Main::need_new_client = true;
    }

    void handle_shutdown(Shutdown_command* shutdown) {
        string message = "Stopped traking.";

        int send_err_code = send(Main::client_socket, message.c_str(), message.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            // TODO: handle
        }

        closesocket(Main::client_socket);

        std::cout << "Message handling: " << message << "\n" << std::endl;
        
        Main::running = false;
    }

    void handle_track(Track_command* track) {
        Command command;
        command.type = Command_type::track;

        // Correctly construct the union member using placement new
        new (&command.data.track) Track_command(*track);

        Command_status status;
        status.handled = false;
        status.command = command;

        Main::command_queue.push_back(status);

        string message = "The procided process will be added to the list of tracke processes.";
        int send_err_code = send(Main::client_socket, message.c_str(), message.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            // TODO: handle
        }
    }

    void handle_untrack(Untrack_command* untrack) {
        Command command;
        command.type = Command_type::untrack;

        // Correctly construct the union member using placement new
        new (&command.data.untrack) Untrack_command(*untrack);

        Command_status status;
        status.handled = false;
        status.command = command;

        Main::command_queue.push_back(status);

        string message = "The procided process will be removed from the list of tracke processes.";
        int send_err_code = send(Main::client_socket, message.c_str(), message.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            // TODO: handle
        }
    }

    //     #include <chrono>
    //     static json create_json_from_tree_node(G_state::Node* root);
    //     static json create_json_from_tree_node(G_state::Node_copy* root);
    // void handle_grouped_report(Grouped_report_command* report) {
    //     // vector<json> j_roots;
    //     // for (G_state::Node* & root : G_state::roots_for_process_tree) { // These might be invalid pointers
    //     //     auto test = root->process;

    //     //     j_roots.push_back(create_json_from_tree_node(root));
    //     // }

    //     // std::string message_as_str = json(j_roots).dump(4);

    //     // int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
    //     // if (send_err_code == SOCKET_ERROR) {
    //     //     // TODO: handle
    //     // }

    //     auto start_time = std::chrono::steady_clock::now();
    //     G_state::create_copy_tree();
    //     auto end_time = std::chrono::steady_clock::now();

    //     auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    //     std::cout << "Time it took to create a copy_node tree: " << duration.count() << std::endl;

    //     start_time = std::chrono::steady_clock::now();
    //     vector<json> j_roots;
    //     for (G_state::Node_copy* root : G_state::copy_roots__for_process_tree) { // These might be invalid pointers
    //         j_roots.push_back(create_json_from_tree_node(root));
    //     }

    //     std::string message_as_str = json(j_roots).dump(4);

    //     int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
    //     if (send_err_code == SOCKET_ERROR) {
    //         // TODO: handle
    //     }
    //     end_time = std::chrono::steady_clock::now();

    //     duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    //     std::cout << "Time it took to create json for the copy_node tree: " << duration.count() << std::endl;

    //     //std::cout << "Message handling: " << message_as_str << "\n" << std::endl;
    // }


    // == Helpers ========================================================

    static json create_json_from_tree_node(G_state::Node* root) {
        json j_process; 
        convert_to_json(root->process, &j_process);

        json j_node;
        j_node["process"] = j_process;

        vector<json> j_children;
        for (G_state::Node* child_node : root->child_processes_nodes) {
            json j_child;
            j_child["child"] = create_json_from_tree_node(child_node);

            j_children.push_back(j_child);
        }
        j_node["children"] = j_children;

        return j_node;
    }

    static void traverse_tree(G_state::Node* root, int depth = 1) {
        std::cout << "Node: " << root->process->data.exe_name << ", depth: " << depth << std::endl;
        for (G_state::Node* child_node : root->child_processes_nodes) {
            traverse_tree(child_node, depth + 1);
        }
    }


}

















