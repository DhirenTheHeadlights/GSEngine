module gse.runtime:attached_link_impl;

import std;

import :attached_link;
import :engine;

auto gse::set_fatal_pipe(void* const editor_pipe) -> void {}

auto gse::report_fatal_to_editor(const std::source_location& loc, const std::string_view comment) -> void {}

auto gse::drain_editor_pipe(void*& editor_pipe, attached_pipe_reader& reader, engine& e) -> void {}

auto gse::announce_attached_surface(void* const editor_pipe, const attached_surface_message& message) -> void {}

auto gse::attached_process_id() -> std::uint32_t {
	return 0;
}
