module gse.runtime:attached_link;

import std;

import gse.core;
import gse.win32;

import :engine;

namespace gse {
	struct attached_pipe_reader {
		std::array<char, sizeof(attached_input_message)> bytes{};
		std::size_t received = 0;
		std::size_t expected = sizeof(std::uint32_t);
		bool have_magic = false;
	};

	auto drain_editor_pipe(
		win32::HANDLE& editor_pipe,
		attached_pipe_reader& reader,
		engine& e
	) -> void;
}
