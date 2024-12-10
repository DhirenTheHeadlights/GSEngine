#include "Core/JsonParser.h"

#include "Platform/PermaAssert.h"

nlohmann::json gse::JsonParse::loadJson(const std::string& path) {
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