module;

#include <fstream>
#include <iostream>
#include "json.hpp"

export module gse.core.json_parser;

export namespace gse::json_parse {
	auto load_json(const std::string& path) -> nlohmann::json;

	// Generic function to parse JSON objects
	// Pass in a lambda that takes a key and a value
	// And do whatever you want with it
	template <typename Function>
	auto parse(const nlohmann::json& json, Function&& process_element) -> void;

	// Generic function to write to a json file
	// Pass in a lambda that takes a key and value
	// And write it however you want
	template <typename Function>
	auto write_json(const std::string& path, Function&& process_element) -> void;
}

import gse.platform.perma_assert;

auto gse::json_parse::load_json(const std::string& path) -> nlohmann::json {
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

template <typename Function>
auto gse::json_parse::parse(const nlohmann::json& json, Function&& process_element) -> void {
	for (const auto& [key, value] : json.items()) {
		std::forward<Function>(process_element)(key, value);
	}
}

template <typename Function>
auto gse::json_parse::write_json(const std::string& path, Function&& process_element) -> void {
	nlohmann::json json;
	process_element(json);

	std::ofstream file(path);
	perma_assert(file.is_open(), "Trying to write to a json file that is not open");
	file << json.dump(4);
}
