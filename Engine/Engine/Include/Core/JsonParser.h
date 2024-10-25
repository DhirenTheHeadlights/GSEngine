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
		return nlohmann::json::parse(file);
	}

	// Generic JSON parser
	template <typename Function>
	void parse(const nlohmann::json& json, Function&& processElement) {
		for (const auto& [key, value] : json.items()) {
			std::forward<Function>(processElement)(key, value);
		}
	}
}
