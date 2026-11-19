module gse.core:path_case;

import std;

import :id;

auto gse::fold_path_case(std::string& key) -> void {
	std::ranges::transform(key, key.begin(), [](const unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
}
