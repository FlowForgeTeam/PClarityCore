#pragma once
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace G_state {
	extern const char* data_file_name;

	extern json data_as_json;

	extern void set_up_on_startup();
}

