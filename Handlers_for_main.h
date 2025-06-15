#pragma once

#include "Commands.h"
#include "Functions_networking.h"

using std::list;

// TODO(damian): handle out of bounds for message_parts. 

// TOOD(damian): think about namespacing function for both thread,
//				 just to be more sure, that thread use shated data as little as possible.

namespace Main {
    struct Command_status {
    	bool    handled;
	    Command command;
    };

    extern list<Command_status> command_queue; 
    extern bool   running;        
    extern SOCKET client_socket;  
    extern bool   need_new_client;
    extern string error_message;   

    extern void client_thread();
    extern void wait_for_client_to_connect();

    extern void handle_report        (Report_command*   command);
    extern void handle_quit          (Quit_command*     commnad);
    extern void handle_shutdown      (Shutdown_command* commnad);
    extern void handle_track         (Track_command*    commnad);
    extern void handle_untrack       (Untrack_command*  commnad);
    extern void handle_grouped_report(Grouped_report_command* command);

}




















