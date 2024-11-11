#pragma once

#include "json.hpp"
#include <concepts>
#include <type_traits>
#include <fstream>

namespace Engine::JsonParse {
	nlohmann::json loadJson(const std::string& path);

	// Generic function to parse JSON objects
	// Pass in a lambda that takes a key and a value
	// And do whatever the fuck you want with it
	template <typename Function>
	void parse(const nlohmann::json& json, Function&& processElement) {
		for (const auto& [key, value] : json.items()) {
			std::forward<Function>(processElement)(key, value);
		}
	}

	// Generic function to write to a json file
	// Pass in a lambda that takes a key and value
	// And write it however the fuck you want
	template <typename Function>
	void writeJson(const std::string& path, Function&& processElement) {
		nlohmann::json json;
		processElement(json);

		std::ofstream file(path);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file: " + path);
		}
		file << json.dump(4);
	}
}
