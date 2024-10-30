#pragma once

#include "json.hpp"
#include <concepts>
#include <type_traits>
#include <fstream>

#include "Platform/PermaAssert.h"

namespace Engine::JsonParse {
	// Load JSON from file
	inline nlohmann::json loadJson(const std::string& path) {
		std::ifstream file(path);
		permaAssertComment(file.is_open(), std::string("Failed to open file: " + path).c_str());
		try {
			return nlohmann::json::parse(file);
		}
		catch (const nlohmann::json::parse_error& e) {
			std::cerr << "JSON parse error: " << e.what() << '\n';
			return nlohmann::json{}; // Return an empty JSON object on failure
		}
	}

	// Generic JSON parser
	template <typename Function>
	void parse(const nlohmann::json& json, Function&& processElement) {
		for (const auto& [key, value] : json.items()) {
			std::forward<Function>(processElement)(key, value);
		}
	}

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
