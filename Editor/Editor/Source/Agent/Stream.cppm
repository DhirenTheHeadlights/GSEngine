export module gse.ide.agent:stream;

import std;
import gse;

import gse.ide.analysis;

import :model;

namespace gse::ide::agent {
	auto escape_json(
		std::string_view text
	) -> std::string;

	auto encode_base64(
		std::span<const std::byte> bytes
	) -> std::string;

	auto read_file_bytes(
		const std::filesystem::path& path
	) -> std::vector<std::byte>;

	auto lowered_extension(
		const std::filesystem::path& path
	) -> std::string;

	auto sendable_encoding(
		const std::filesystem::path& path
	) -> bool;

	auto media_type_for(
		const std::filesystem::path& path
	) -> std::string_view;

	auto user_message(
		std::string_view prompt,
		std::span<const attachment> attachments
	) -> std::string;

	auto string_at(
		const analysis::json::value& parent,
		std::string_view key
	) -> std::string_view;

	auto int_at(
		const analysis::json::value& parent,
		std::string_view key
	) -> std::int64_t;

	auto record_usage(
		const analysis::json::value& message,
		session_info& info
	) -> void;

	auto summarize(
		const analysis::json::value& event,
		session_info& info
	) -> std::vector<transcript_row>;

	auto collect_denials(
		const analysis::json::value& event,
		std::vector<transcript_row>& out
	) -> void;

	auto to_lines(
		std::string_view text
	) -> std::vector<std::string>;

	auto tool_row(
		const analysis::json::value& block
	) -> transcript_row;

	auto jump_line_for(
		const transcript_row& row
	) -> std::uint32_t;

	auto turn_failed(
		const analysis::json::value& event
	) -> bool;

	auto retryable_failure(
		const analysis::json::value& event
	) -> bool;

	auto limit_reset_at(
		const analysis::json::value& event
	) -> std::int64_t;

	auto usage_limited(
		const analysis::json::value& event
	) -> bool;


	auto anchorable(
		const analysis::json::value& event
	) -> bool;

	auto user_rows(
		const analysis::json::value& event
	) -> std::vector<transcript_row>;

	auto tool_action(
		const transcript_row& row
	) -> std::string;
}
