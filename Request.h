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
    pc_time = 6
};

struct Report_request {};

struct Quit_request {};

struct Shutdown_request {};

struct Track_request { string path; };

struct Untrack_request { string path;};

struct Grouped_report_request {};

struct Pc_time_request {};


struct Request {
    variant<Report_request,
            Quit_request,
            Shutdown_request,
            Track_request,
            Untrack_request,
            Grouped_report_request,
            Pc_time_request> variant; 
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












