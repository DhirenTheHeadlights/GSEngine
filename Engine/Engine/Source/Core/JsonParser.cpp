module;

#include "json.hpp"
#include <fstream>
#include <iostream>

module gse.core.json_parser;

import gse.platform.perma_assert;

nlohmann::json gse::json_parse::load_json(const std::string& path) {
	std::ifstream file(path);
	assert_comment(file.is_open(), std::string("Failed to open file: " + path).c_str());
	try {
		return nlohmann::json::parse(file);
	}
	catch (const nlohmann::json::parse_error& e) {
		std::cerr << "JSON parse error: " << e.what() << '\n';
		return nlohmann::json{}; // Return an empty JSON object on failure
	}
}

//template <typename Function>
//auto gse::json_parse::