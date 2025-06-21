#include "Request.h"
#include <new>
#include <utility>
using namespace std;

pair<Request_type, bool> request_type_from_int(int id) {
    switch (id) {
        case 0:  return pair(Request_type::report,         true);
        case 1:  return pair(Request_type::quit,           true);
        case 2:  return pair(Request_type::shutdown,       true);
        case 3:  return pair(Request_type::track,          true);
        case 4:  return pair(Request_type::untrack,        true);
        case 5:  return pair(Request_type::grouped_report, true);
        case 6:  return pair(Request_type::pc_time,        true);

        default: return pair(Request_type::report,         false);
    }
}

pair<Request, bool> request_from_json(const char* json_as_c_str) {
    json j = json::parse(json_as_c_str, nullptr, false);
    if (j.is_discarded() || !j.contains("request_id") || !j["request_id"].is_number()) {
        return pair(Request(), false);
    }

    auto result = request_type_from_int(j["request_id"]);
    if (!result.second) return pair(Request(), false);

    switch (result.first) {
        case Request_type::report:         return report(&j);
        case Request_type::quit:           return quit(&j);
        case Request_type::shutdown:       return shutdown(&j);
        case Request_type::track:          return track(&j);
        case Request_type::untrack:        return untrack(&j);
        case Request_type::grouped_report: return grouped_report(&j);
        case Request_type::pc_time:        return pc_time(&j);
        default:                           return pair(Request(), false);
    }
}

pair<Request, bool> report(json* j) {
    Request request = {};
    request.variant = Report_request{};
    
    return pair(request, true);
}

pair<Request, bool> quit(json* j) {
    Request request = {};
    request.variant = Quit_request{}; 
    
    return pair(request, true);
}

pair<Request, bool> shutdown(json* j) {
    Request request = {};
    request.variant = Shutdown_request{}; 
    
    return pair(request, true);
}

pair<Request, bool> track(json* j) {
    if (!j->contains("extra") || !(*j)["extra"].contains("path") || !(*j)["extra"]["path"].is_string()) {
        return pair(Request(), false);
    }

    Track_request track = {};
    track.path          = (*j)["extra"]["path"];

    Request request = {};
    request.variant = track;

    return pair(request, true);
}

pair<Request, bool> untrack(json* j) {
    if (!j->contains("extra") || !(*j)["extra"].contains("path") || !(*j)["extra"]["path"].is_string()) {
        return pair(Request(), false);
    }

    Track_request track = {};
    track.path          = (*j)["extra"]["path"];

    Request request = {};
    request.variant = track;
    
    return pair(request, true);
}

pair<Request, bool> grouped_report(json* j) {
    Request request = {};
    request.variant = Grouped_report_request{};

    return pair(request, true);
}

pair<Request, bool> pc_time(json* j) {
	Request request = {};
    request.variant = Pc_time_request{};

    return pair(request, true);
}


































