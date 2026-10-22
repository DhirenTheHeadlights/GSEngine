module gse.runtime:attached_link;

import std;

import gse.core;

import :engine;

namespace gse {
	struct attached_pipe_reader {
		std::array<char, std::max(sizeof(attached_input_message), sizeof(attached_resize_message))> bytes{};
		std::size_t received = 0;
		std::size_t expected = sizeof(std::uint32_t);
		bool have_magic = false;
	};

	auto drain_editor_pipe(
		void*& editor_pipe,
		attached_pipe_reader& reader,
		engine& e
	) -> void;

	auto set_fatal_pipe(
		void* editor_pipe
	) -> void;

	auto report_fatal_to_editor(
		const std::source_location& loc,
		std::string_view comment
	) -> void;

	auto announce_attached_surface(
		void* editor_pipe,
		const attached_surface_message& message
	) -> void;

	[[nodiscard]] auto attached_process_id() -> std::uint32_t;
}
