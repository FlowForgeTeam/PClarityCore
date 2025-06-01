#include <fstream>
#include "Functions_file_system.h"
#include "Global_state.h"

namespace G_state {
    const char* data_file_name = "data.json";
	json data_as_json;

	void set_up_on_startup() {
        // TODO: dont like all this error handleing here
        std::error_code err_code;

        bool data_exists = fs::exists(G_state::data_file_name, err_code);
        if (err_code) {
            // error handle
        }

        if (data_exists) { // Read it, store it
            std::string text;
            read_file(G_state::data_file_name, &text);

            G_state::data_as_json = json::parse(text);
        }
        else { // Create and store
            std::ofstream new_file(G_state::data_file_name);
            new_file << "{ }" << std::endl;
            new_file.close();
        }

    }
    
}