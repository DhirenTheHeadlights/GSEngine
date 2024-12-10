#pragma once

#include "json.hpp"
#include <concepts>
#include <type_traits>
#include <fstream>

namespace gse::json_parse {
	nlohmann::json load_json(const std::string& path);

	// Generic function to parse JSON objects
	// Pass in a lambda that takes a key and a value
	// And do whatever the fuck you want with it
	template <typename function>
	void parse(const nlohmann::json& json, function&& process_element) {
		for (const auto& [key, value] : json.items()) {
			std::forward<function>(process_element)(key, value);
		}
	}

	// Generic function to write to a json file
	// Pass in a lambda that takes a key and value
	// And write it however the fuck you want
	template <typename function>
	void writeJson(const std::string& path, function&& process_element) {
		nlohmann::json json;
		process_element(json);

		std::ofstream file(path);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file: " + path);
		}
		file << json.dump(4);
	}
}
