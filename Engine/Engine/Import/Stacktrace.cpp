module gse.stacktrace;

import std;

auto gse::clean_symbol(std::string symbol) -> std::string {
	for (std::size_t at = symbol.find('@'); at != std::string::npos; at = symbol.find('@', at)) {
		std::size_t end = at + 1;
		while (end < symbol.size()) {
			const char c = symbol[end];
			const bool module_char = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '.';
			if (!module_char) {
				break;
			}
			++end;
		}
		symbol.erase(at, end - at);
	}

	const auto replace_all = [&symbol](const std::string_view from, const std::string_view to) {
		for (std::size_t p = symbol.find(from); p != std::string::npos; p = symbol.find(from, p + to.size())) {
			symbol.replace(p, from.size(), to);
		}
	};

	replace_all("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string");
	replace_all("std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string");
	replace_all("std::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string");
	replace_all("std::basic_string<char, std::char_traits<char>, std::allocator<char>>", "std::string");
	replace_all("std::basic_string_view<char, std::char_traits<char> >", "std::string_view");
	replace_all("std::basic_string_view<char, std::char_traits<char>>", "std::string_view");
	replace_all("[abi:cxx11]", "");
	replace_all("__cxx11::", "");
	return symbol;
}
