export module gse.core.json_parse;

import std;

import <json.hpp>;

export namespace gse::json_parse {
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
	void write_json(const std::string& path, function&& process_element) {
		nlohmann::json json;
		process_element(json);

		std::ofstream file(path);
		if (!file.is_open()) {
			throw std::runtime_error("Failed to open file: " + path);
		}
		file << json.dump(4);
	}
}

nlohmann::json gse::json_parse::load_json(const std::string& path) {
	std::ifstream file(path);
	if (!file.is_open()) {
		std::cerr << "Json file not open\n";
		return {};
	}
	try {
		return nlohmann::json::parse(file);
	}
	catch (const nlohmann::json::parse_error& e) {
		std::cerr << "JSON parse error: " << e.what() << '\n';
		return nlohmann::json{}; // Return an empty JSON object on failure
	}
}
