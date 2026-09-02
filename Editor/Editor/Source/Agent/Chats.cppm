export module gse.ide.agent:chats;

import std;
import gse;

import :model;

namespace gse::ide::agent {
	constexpr std::size_t uuid_length = 36;
	constexpr std::size_t chat_title_columns = 28;
	constexpr std::size_t summary_scan_lines = 64;
	constexpr std::size_t summary_scan_bytes = 1u << 20;

	auto transcript_path(
		const std::string& agent_id
	) -> std::filesystem::path;

	auto transcript_dir(
		const std::filesystem::path& cwd
	) -> std::filesystem::path;

	auto first_line(
		std::string_view text
	) -> std::string_view;

	auto chat_summary(
		const std::filesystem::path& file
	) -> std::string;

	auto chat_title(
		std::string_view summary
	) -> std::string;

	auto past_chats(
		const std::filesystem::path& cwd
	) -> std::vector<past_chat>;

	auto restore_rows(
		session& s
	) -> bool;

	auto hydrate_session(
		session& s
	) -> void;

	auto restore_chat(
		data& d,
		const past_chat& chat
	) -> void;

	auto entry_uuid(
		std::string_view line
	) -> std::string_view;

	auto entry_parent(
		std::string_view line
	) -> std::string_view;

	auto forked_session_id() -> std::string;

	auto substituted(
		std::string_view line,
		std::string_view from,
		std::string_view to
	) -> std::string;

	auto write_fork(
		const std::filesystem::path& source,
		const std::filesystem::path& target,
		std::string_view leaf,
		std::string_view forked_id
	) -> bool;

	auto rewind_anchor(
		const session& s,
		std::uint32_t row
	) -> std::optional<std::uint32_t>;

	auto rewind_session(
		session& s,
		std::uint32_t row
	) -> bool;
}
