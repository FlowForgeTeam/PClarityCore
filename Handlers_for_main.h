#pragma once

#include "Request.h"
#include "Functions_networking.h"

using std::list;

// TODO(damian): handle out of bounds for message_parts. 

// TOOD(damian): think about namespacing function for both thread,
//				 just to be more sure, that thread use shared data as little as possible.

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

    // TODO(damian): maybe this has to be on the data thread side, move this into the G_state.
    extern list<Request_status> request_queue; 

    extern bool   running;        
    extern SOCKET client_socket;  
    extern bool   need_new_client;
    extern string error_message;   

    extern void client_thread();
    extern void wait_for_client_to_connect();
    extern void handle_socker_error();

    extern void handle_report          (Report_request*           request);
    extern void handle_quit            (Quit_request*             request);
    //extern void handle_shutdown        (Shutdown_request*         request);
    extern void handle_track           (Track_request*            request);
    extern void handle_untrack         (Untrack_request*          request);
    extern void handle_grouped_report  (Grouped_report_request*   request);
    extern void handle_pc_time         (Pc_time_request*          request);
    extern void handle_report_apps_only(Report_apps_only_request* request);

}




















