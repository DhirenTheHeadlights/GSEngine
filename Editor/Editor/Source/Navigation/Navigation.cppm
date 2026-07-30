export module gse.ide.navigation:navigation;

import std;

export namespace gse::ide {
	struct jump_to_request {
		std::filesystem::path path;
		std::uint32_t line = 0;
		std::uint32_t column = 0;
	};
}
