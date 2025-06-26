#pragma once
#include <string>
#include <utility>
#include <variant>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using std::pair;
using std::variant;
using std::string;

enum class Request_type {
    report   = 0,
    quit     = 1,
    shutdown = 2,
    track    = 3,
    untrack  = 4,
    grouped_report = 5,
    pc_time     = 6,
    report_apps_only = 7,
    tracked_only = 8,
    change_update_time = 9,
};

struct Report_request {};

struct Quit_request {};

struct Shutdown_request {};

struct Track_request { string path; };

struct Untrack_request { string path; };

struct Grouped_report_request {};

struct Pc_time_request {};

struct Report_apps_only_request {};

struct Report_tracked_only {};

struct Change_update_time { uint32_t duration_in_sec;  };

struct Request {
    variant<Report_request,
            Quit_request,
            Shutdown_request,
            Track_request,
            Untrack_request,
            Grouped_report_request,
            Pc_time_request,
            Report_apps_only_request,
            Report_tracked_only,
            Change_update_time> variant; 
};

pair<Request_type, bool> request_type_from_int(int id);
pair<Request, bool> request_from_json(const char* json_as_c_str);
pair<Request, bool> report        (json* j);
pair<Request, bool> quit          (json* j);
pair<Request, bool> shutdown      (json* j);
pair<Request, bool> track         (json* j);
pair<Request, bool> untrack       (json* j);
pair<Request, bool> grouped_report(json* j);
pair<Request, bool> pc_time       (json* j);
pair<Request, bool> report_apps_only(json* j);
pair<Request, bool> report_tracked_only(json* j);
pair<Request, bool> change_update_time(json* j);















