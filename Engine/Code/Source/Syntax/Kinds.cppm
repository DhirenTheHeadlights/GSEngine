export module gse.syntax:kinds;

import std;

export namespace gse::syntax {
	struct kind_info {
		std::uint32_t color = 0;
	};

	enum class kind {
		comment [[= kind_info{ .color = 0x4b5b7e }]],
		keyword [[= kind_info{ .color = 0x2a7195 }]],
		control_keyword [[= kind_info{ .color = 0xd57192 }]],
		literal [[= kind_info{ .color = 0x9586df }]],
		number [[= kind_info{ .color = 0x9586df }]],
		string [[= kind_info{ .color = 0x9586df }]],
		preprocessor [[= kind_info{ .color = 0x2b3959 }]],
		punctuation [[= kind_info{ .color = 0x9ca5b8 }]]
	};
}
