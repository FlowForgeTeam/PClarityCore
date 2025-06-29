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
    track    = 2,
    untrack  = 3,
    grouped_report = 4,
    pc_time     = 5,
    report_apps_only = 6,
    tracked_only = 7,
    change_update_time = 8,
};

struct Report_request {};

struct Quit_request {};

struct Track_request { string path; };

struct Untrack_request { string path; };

struct Grouped_report_request {};

struct Pc_time_request {};

struct Report_apps_only_request {};

struct Report_tracked_only {};

struct Change_update_time_request { uint32_t duration_in_sec;  };

struct Request {
    variant<Report_request,
            Quit_request,
            Track_request,
            Untrack_request,
            Grouped_report_request,
            Pc_time_request,
            Report_apps_only_request,
            Report_tracked_only,
            Change_update_time_request> variant; 
};

pair<Request_type, bool> request_type_from_int(int id);
pair<Request, bool> request_from_json(const char* json_as_c_str);

pair<Request, bool> report             (json* j);
pair<Request, bool> quit               (json* j);
pair<Request, bool> track              (json* j);
pair<Request, bool> untrack            (json* j);
pair<Request, bool> grouped_report     (json* j);
pair<Request, bool> pc_time            (json* j);
pair<Request, bool> report_apps_only   (json* j);
pair<Request, bool> report_tracked_only(json* j);
pair<Request, bool> change_update_time (json* j);















