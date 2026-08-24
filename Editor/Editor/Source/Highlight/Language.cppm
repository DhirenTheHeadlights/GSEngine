export module gse.ide.highlight:language;

import std;

export namespace gse::ide {
	enum class document_language : std::uint8_t {
		plain,
		cpp,
		markdown,
	};
}
