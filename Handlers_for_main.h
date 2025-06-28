#pragma once

#include "Request.h"
#include "Functions_networking.h"

using std::list;

namespace Client {
    struct Request_status {
        bool    handled;
	    Request request;
    };
    
    struct Data_thread_error_status {
        bool handled;
        G_state::Error error;
    };

    // Only data thread can add new entries here. Client thread will use them and mark as handled.
    extern list<Data_thread_error_status> data_thread_error_queue;

    // TODO(damian): maybe this beeing on the data thread side would make more sense.
    extern list<Request_status> request_queue; 

    extern bool   client_running;       

    extern optional<G_state::Error> fatal_error;

    extern SOCKET client_socket;  
    extern bool   need_new_client;
    extern string error_message;   

    extern void client_thread();
    extern void wait_for_client_to_connect();
    
    extern void handle_socker_error();

    extern Error handle_report             (Report_request*           request);
    extern void  handle_quit               (Quit_request*             request);
    //extern void handle_shutdown          (Shutdown_request*         request);
    extern void  handle_track              (Track_request*            request);
    extern void  handle_untrack            (Untrack_request*          request);
    extern Error handle_grouped_report     (Grouped_report_request*   request);
    extern void  handle_pc_time            (Pc_time_request*          request);
    extern Error handle_report_apps_only   (Report_apps_only_request* request);
    extern Error handle_report_tracked_only(Report_tracked_only*      request);
    extern void  handle_change_update_time (Change_update_time*       request);


      
}




















