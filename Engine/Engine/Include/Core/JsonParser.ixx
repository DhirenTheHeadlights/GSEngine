//module;
//
//#include <fstream>
//#include <iostream>
//#include "json.hpp"

export module gse.core.json_parser;

//import gse.platform.assert;
//
//export namespace gse::json_parse {
//	auto load_json(const std::filesystem::path& path) -> nlohmann::json;
//
//	// Generic function to parse JSON objects
//	// Pass in a lambda that takes a key and a value
//	// And do whatever the fuck you want with it
//	template <typename Function>
//	auto parse(const nlohmann::json& json, Function&& process_element) -> void {
//		for (const auto& [key, value] : json.items()) {
//			std::forward<Function>(process_element)(key, value);
//		}
//	}
//
//	// Generic function to write to a json file
//	// Pass in a lambda that takes a key and value
//	// And write it however the fuck you want
//	template <typename Function>
//	auto write_json(const std::string& path, Function&& process_element) -> void {
//		nlohmann::json json;
//		process_element(json);
//
//		std::ofstream file(path);
//		if (!file.is_open()) {
//			throw std::runtime_error("Failed to open file: " + path);
//		}
//		file << json.dump(4);
//	}
//}
//
//auto gse::json_parse::load_json(const std::filesystem::path& path) -> nlohmann::json {
//	std::ifstream file(path);
//	assert_comment(file.is_open(), std::string("Failed to open file: " + path.string()).c_str());
//	try {
//		return nlohmann::json::parse(file);
//	}
//	catch (const nlohmann::json::parse_error& e) {
//		std::cerr << "JSON parse error: " << e.what() << '\n';
//		return nlohmann::json{}; // Return an empty JSON object on failure
//	}
//}
