#include "Handlers_for_main.h"
#include <fstream>
#include <thread>

// This is file private
struct Process_node {
    Process_data*         process;
	vector<Process_node*> child_processes_nodes;
};

namespace Client {

    list<Request_status> request_queue; 

    bool   client_running  = true;
    optional<G_state::Error> fatal_error;
    
    SOCKET client_socket   = {0} ; // NOTE(damian): maybe use optional here, not sure yet.
    bool   need_new_client = true;

    list<Data_thread_error_status> data_thread_error_queue;
    
    static json create_json_from_tree_node(Process_node* root);

    static void create_process_tree(vector<Process_node*>* roots, vector<Process_data>* data);
    static void free_process_tree  (vector<Process_node*>* roots);
    static void free_tree_memory   (Process_node* root);

    static Data_thread_error_status* error_request_p = nullptr; // NOTE(damian): hate it here.

    static void create_responce(G_state::Error* err, json* data, json* responce);
    static void create_responce_for_invalid(json* responce); // TODO(damian): dont like this, 1 func should be able to handle both valid and invalid commands.

    void client_thread() {
        while (client_running) {
            if (need_new_client) {
                Client::wait_for_client_to_connect();
                need_new_client = false;
            }

            // Clearing out the request queue.
            for (auto it = request_queue.begin(); it != request_queue.end();) {
                if (it->handled)
                    it = request_queue.erase(it);
                else
                    ++it;
            }

            // TODO(damian): its stupid to have sigle message handeling at a time but also multiple message deletion each iteration. 

            // =======================================================================================
            // TODO(damian): command_queue can can only have 2 parts. (HANDLED-UNHANDLED).
            //				 assert to make sure there is not mix up in the middle.
            bool is_handled = false;
			for (Request_status& status : request_queue) {
                if (status.handled && !is_handled) {
                    is_handled = true;
                }
				else if (!status.handled && is_handled) {
					std::cout << "Error: command queue has a mix of handled and unhandled commands." << std::endl;
					assert(false);
				}
            }
            // =======================================================================================

            auto p_to_error    = Client::data_thread_error_queue.begin(); 
            for (; p_to_error != Client::data_thread_error_queue.end(); ) {
                if (!p_to_error->handled) {
                    error_request_p = &(*p_to_error); // TODO: maybe move here
                    break;
                }
                else {
                    p_to_error = Client::data_thread_error_queue.erase(p_to_error);
                }
            }

            const size_t receive_buffer_size = 512;
            char receive_buffer[receive_buffer_size];

            int n_bytes_returned = recv(client_socket, receive_buffer, receive_buffer_size, NULL); // TODO(damian): add a timeout here.
            if (n_bytes_returned == SOCKET_ERROR) {
                Client::handle_socker_error();
                continue;
            }
            if (n_bytes_returned > receive_buffer_size - 1) { // Messege overflow.
                // TODO: Add something that tells the client that the request is too long.
                json responce;
                create_responce_for_invalid(&responce);

                string messege = std::move(responce.dump(4));

                int send_err_code   = send(client_socket, messege.c_str(), messege.size(), NULL);
                if (send_err_code == SOCKET_ERROR) {
                    Client::handle_socker_error();
                }
            }
            receive_buffer[n_bytes_returned] = '\0';

            std::cout << "Received a message of " << n_bytes_returned << " bytes." << std::endl;
            std::cout << "Message: " << "'" << receive_buffer << "'" << "\n" << std::endl;

            if (fatal_error.has_value()) { 
                json responce;
                create_responce(&fatal_error.value(), nullptr, &responce);

                string responce_as_str = responce.dump(4);
                std::cout << "Message --> " << responce_as_str << std::endl;

                int send_err_code = send(client_socket, responce_as_str.c_str(), responce_as_str.size(), NULL);
                if (send_err_code == SOCKET_ERROR) {
                    Client::handle_socker_error();                
                }

                closesocket(client_socket);
                Client::client_running = false;
            }
            else {
                // Parsing the request.
                std::pair<Request, bool> result = request_from_json(receive_buffer);
                if (!result.second) {
                    
                    json responce;
                    create_responce_for_invalid(&responce);

                    string messege = std::move(responce.dump(4));

                    int send_err_code   = send(client_socket, messege.c_str(), messege.size(), NULL);
                    if (send_err_code == SOCKET_ERROR) {
                        Client::handle_socker_error();
                    }
                }
                else {
                    Report_request*           report         = std::get_if<Report_request>          (&result.first.variant);
                    Quit_request*             quit           = std::get_if<Quit_request>            (&result.first.variant);
                    Shutdown_request*         shutdown       = std::get_if<Shutdown_request>        (&result.first.variant);
                    Track_request*            track          = std::get_if<Track_request>           (&result.first.variant);
                    Untrack_request*          untrack        = std::get_if<Untrack_request>         (&result.first.variant);
                    Grouped_report_request*   grouped_report = std::get_if<Grouped_report_request>  (&result.first.variant);
                    Pc_time_request*          pc_time        = std::get_if<Pc_time_request>         (&result.first.variant);
                    Report_apps_only_request* apps_only      = std::get_if<Report_apps_only_request>(&result.first.variant);
                    Report_tracked_only*      tracked_only   = std::get_if<Report_tracked_only>(&result.first.variant);
                    Change_update_time*       change_time    = std::get_if<Change_update_time>(&result.first.variant);

                    if (report != nullptr) {
                        Client::handle_report(report);
                    } 
                    else if (quit != nullptr) {
                        Client::handle_quit(quit);
                    }
                    else if (shutdown != nullptr) {
                        assert(false);
                        // Client::handle_shutdown(shutdown);
                    } 
                    else if (track != nullptr) {
                        Client::handle_track(track);
                    }
                    else if (untrack != nullptr) {
                        Client::handle_untrack(untrack);
                    }
                    else if (grouped_report != nullptr) {
                        Client::handle_grouped_report(grouped_report);
                    }
                    else if (pc_time != nullptr) {
                        Client::handle_pc_time(pc_time);
                    }
                    else if (apps_only != nullptr) {
                        Client::handle_report_apps_only(apps_only);
                    }
                    else if (tracked_only != nullptr) {
                        Client::handle_report_tracked_only(tracked_only);
                    }
                    else if (change_time != nullptr) {
                        Client::handle_change_update_time(change_time);
                    }
                    else {
                        assert(false);
                    }

                }

                if (error_request_p) {
                    error_request_p->handled = true;
                    error_request_p          = nullptr;
                }

                // NOTE(damian): clinet should not me managinf data (files). The main process has to be doing it.
            }
            
        }
    }

    void wait_for_client_to_connect() {
        std::cout << "Waiting for a new connection with a client." << std::endl;
        //pair<SOCKET, G_state::Error> result = initialise_tcp_connection_with_client();
        // if (result.second.type == G_state::Error_type::ok) {
        //     Client::client_socket = result.first;
        //     std::cout << "Client connected. \n" << std::endl;
        // }
        // else {
            std::cout << "Error trying to initialise connection with a client." << std::endl;
            fatal_error = G_state::Error(G_state::Error_type::tcp_initialisation_failed);
        // }
    }

    void handle_socker_error() {
        std::cout << "err_code: " << WSAGetLastError() << std::endl;
        closesocket(Client::client_socket);
        Client::need_new_client = true;
    }

    static void create_responce_for_invalid(json* responce) {
        // TODO(damian): make -1 a constant. 
        (*responce)["err_code"] = -1;
        (*responce)["data"]     = json::object(); 
        (*responce)["messege"]  = "Invalid command";
    }

    static void create_responce(G_state::Error* err, json* data, json* responce) {
        (*responce)["err_code"] = err->type;
        (*responce)["data"]     = (data == nullptr ? nullptr : *data);   
        (*responce)["messege"]  = err->message;
    }

    void handle_report(Report_request* report) {
        assert(!G_state::Client_data::need_data);

        // Telling the data thread to produce a copy of thread safe data for us to use 
        G_state::Client_data::need_data = true;
        while(G_state::Client_data::need_data);         

        vector<json> j_tracked;
        for (Process_data& data : G_state::Client_data::maybe_data.value().copy_tracked_processes) {
            json temp;
            convert_to_json(&data, &temp);
            j_tracked.push_back(temp);
        }

        vector<json> j_cur_active;
        for (Process_data& data : G_state::Client_data::maybe_data.value().copy_currently_active_processes) {
            json temp;
            convert_to_json(&data, &temp);
            j_cur_active.push_back(temp);
        }

        json j_data;
        j_data["tracked"]          = j_tracked;
        j_data["currently_active"] = j_cur_active;

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);

        int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error();                
        }

    }

    void handle_quit(Quit_request* quit) {
        json j_data = json::object();

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);

        int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error();
        }

        closesocket(Client::client_socket);

        Client::need_new_client = true;
    }

    // NOTE(damian): dont know if we even need this.
    //void handle_shutdown(Shutdown_request* shutdown) {
    //    json j_data = json::object();

    //    G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
    //    json responce;
    //    create_responce(&err, &j_data, &responce);

    //    std::string message = responce.dump(4);

    //    int send_err_code = send(Client::client_socket, message.c_str(), message.length(), NULL);
    //    if (send_err_code == SOCKET_ERROR) {
    //        Client::handle_socker_error(); 
    //    }

    //    closesocket(Client::client_socket);

    //    std::cout << "Message handling: " << message << "\n" << std::endl;
    //    
    //    Client::running = false;
    //}

    void handle_track(Track_request* track) {
        Request request = {};
        request.variant = *track;

        Request_status status;
        status.handled = false;
        status.request = request;

        Client::request_queue.push_back(status);

        json j_data; // Nothing inside data json part for this request.

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);
        int send_err_code = send(Client::client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error(); 
        }

    }

    void handle_untrack(Untrack_request* untrack) {
        Request request = {};
        request.variant = *untrack;

        Request_status status;
        status.handled = false;
        status.request = request;

        Client::request_queue.push_back(status);

        json j_data; // Nothing inside data json part for this request.

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);
        int send_err_code = send(Client::client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error(); 
        }

    }

    void handle_grouped_report(Grouped_report_request* report) {
       assert(!G_state::Client_data::need_data);            

       G_state::Client_data::need_data = true;

       while(G_state::Client_data::need_data);    

       G_state::Client_data::Data* data = &G_state::Client_data::maybe_data.value();

       vector<json> j_tracked;
       for (Process_data& data : data->copy_tracked_processes) {
           json j_data;
           convert_to_json(&data, &j_data);
           j_tracked.push_back(j_data);
       }

       vector<Process_node*> roots;
       create_process_tree(&roots, &data->copy_currently_active_processes);

       vector<json> j_roots;
       for (Process_node* root : roots) { 
           j_roots.push_back(create_json_from_tree_node(root)); 
       }

       json j_data;
       j_data["tracked"] = j_tracked;
       j_data["active"]  = j_roots;

       G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
       json responce;
       create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);

       int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
       if (send_err_code == SOCKET_ERROR) {
           Client::handle_socker_error(); 
       }

       free_process_tree(&roots);
    }

    void handle_pc_time(Pc_time_request* request) {
        json system_json;
        system_json["up_time_ms"] = G_state::Dynamic_system_info::up_time;

        json j_time_utc;
		j_time_utc["year"]   = G_state::Dynamic_system_info::system_time.wYear;
        j_time_utc["month"]  = G_state::Dynamic_system_info::system_time.wMonth;
        j_time_utc["day"]    = G_state::Dynamic_system_info::system_time.wDay;
        j_time_utc["hour"]   = G_state::Dynamic_system_info::system_time.wHour;
        j_time_utc["minute"] = G_state::Dynamic_system_info::system_time.wMinute;
        j_time_utc["second"] = G_state::Dynamic_system_info::system_time.wSecond;

        system_json["system_time_utc"] = j_time_utc;
           
        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
        json responce;
        create_responce(&err, &system_json, &responce);

        std::string message_as_str = responce.dump(4);

        int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error();                
        }

    }

    void handle_report_apps_only(Report_apps_only_request* report_apps_only) {
        assert(!G_state::Client_data::need_data);

        // Telling the data thread to produce a copy of thread safe data for us to use 
        G_state::Client_data::need_data = true;
        while(G_state::Client_data::need_data);         

        vector<json> j_tracked;
        for (Process_data& data : G_state::Client_data::maybe_data.value().copy_tracked_processes) {
            json temp;
            convert_to_json(&data, &temp);
            j_tracked.push_back(temp);
        }

        bool is_set = false;
        DWORD explorer_pid = 0;
        
        // Getting pid for explorer.exe from tracked.
        for (Process_data& data : G_state::Client_data::maybe_data.value().copy_tracked_processes) {
            if (!data.snapshot.has_value()) continue;
            if (data.snapshot.value().exe_name == "explorer.exe") {
                explorer_pid = data.snapshot.value().pid;
                is_set = true;
                break;
            }
        }

        // Getting pid for explorer.exe from ective apps.
        if (!is_set) {
            for (Process_data& data : G_state::Client_data::maybe_data.value().copy_currently_active_processes) {
                if (!data.snapshot.has_value()) continue;
                if (data.snapshot.value().exe_name == "explorer.exe") {
                    explorer_pid = data.snapshot.value().pid;
                    is_set = true;
                    break;
                }
            }
        }

        // Jsoning
        vector<json> j_apps;
        for (Process_data& data : G_state::Client_data::maybe_data.value().copy_currently_active_processes) {
            if (!data.snapshot.has_value()) continue;
            if (data.snapshot.value().ppid == explorer_pid) {
                json temp;
                convert_to_json(&data, &temp);
                j_apps.push_back(temp);
            }
        }

        json j_data;
        j_data["tracked"] = j_tracked;
        j_data["apps"]    = j_apps;

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{G_state::Error_type::ok};
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);

        int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error();                
        }

    }

    void handle_report_tracked_only(Report_tracked_only* request) {
        assert(!G_state::Client_data::need_data);

        // Telling the data thread to produce a copy of thread safe data for us to use 
        G_state::Client_data::need_data = true;
        while (G_state::Client_data::need_data);

        vector<json> j_tracked;
        for (Process_data& data : G_state::Client_data::maybe_data.value().copy_tracked_processes) {
            json temp;
            convert_to_json(&data, &temp);
            j_tracked.push_back(temp);
        }

        json j_data;
        j_data["tracked"] = j_tracked;

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{ G_state::Error_type::ok };
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);

        int send_err_code = send(client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error();
        }
    }

    void handle_change_update_time(Change_update_time* update_time) {
        Request request = {};
        request.variant = *update_time;

        Request_status status;
        status.handled = false;
        status.request = request;

        Client::request_queue.push_back(status);

        json j_data; // Nothing inside data json part for this request.

        G_state::Error err = error_request_p ? error_request_p->error : G_state::Error{ G_state::Error_type::ok };
        json responce;
        create_responce(&err, &j_data, &responce);

        std::string message_as_str = responce.dump(4);
        int send_err_code = send(Client::client_socket, message_as_str.c_str(), message_as_str.length(), NULL);
        if (send_err_code == SOCKET_ERROR) {
            Client::handle_socker_error();
        }
    
    }



    // == Helpers ========================================================

    static void create_process_tree(vector<Process_node*>* roots, vector<Process_data>* data) {
        // NOTE(damian): only handling active processes, since they have a hierarchy.
        //               tracked once are provided by the data thread, 
        //               but will be jsoned inside the client thread into a single json list.  

        unordered_map<DWORD, Process_node*> tree;

        // TODO(damian): dont like .value() here inside the loops. It also might throw.

        for (Process_data& process : *data) {
            Process_node* new_node = new Process_node{ &process, vector<Process_node*>() };
            if (new_node == NULL) {
                // TODO(damian): handle better.
                assert(false);
            }

            if (!process.snapshot.has_value()) {
               assert(false);
            }
            tree[process.snapshot.value().pid] = new_node;
        }

        // Fill the tree up with current data
        for (Process_data& process : *data) {
            auto process_node = tree.find(process.snapshot.value().pid);
            if (process_node == tree.end()) {
                // TODO(damian): handle better.
                assert(false); // This cant happend.
            };

            auto parent_node = tree.find(process.snapshot.value().ppid);
            if (parent_node == tree.end()) continue;
            // Its ok, some processes dont have parents. 

            parent_node->second->child_processes_nodes.push_back(process_node->second);
        }

        // Finding root processes
        for (auto& pair : tree) {
            Process_node* node = pair.second;
            DWORD ppid = node->process->snapshot.value().ppid;

            auto it = tree.find(ppid);
            if (it == tree.end()) { // Not found
                roots->push_back(node);
            }
        }
    }

    static void free_process_tree(vector<Process_node*>* roots) {
        // Freeing the dyn allocated memory
        for (Process_node* root : *roots) {
            free_tree_memory(root);
        }

        // No need to remove the dangling pointers here.
        // Called is responsible for it.
        // Also if we create the tree localy inside handle for grouped report, it will be deleted localy as well.
    }

    static void free_tree_memory(Process_node* root) {
        for (Process_node* child_node : root->child_processes_nodes) {
            free_tree_memory(child_node);
        }
        delete root;
    }

    static json create_json_from_tree_node(Process_node* root) {
        if (root == nullptr) {
            assert(false);
        }

        json j_process; 
        convert_to_json(root->process, &j_process);

        json j_node;
        j_node["process"] = j_process;

        vector<json> j_children;
        for (Process_node* child_node : root->child_processes_nodes) {
            json j_child = create_json_from_tree_node(child_node);

            j_children.push_back(j_child);
        }
        j_node["children"] = j_children;

        return j_node;
    }

}

















