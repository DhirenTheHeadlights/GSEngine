export module gse.ide.agent:blame;

import std;
import gse;

import gse.ide.build;

import :model;

namespace gse::ide::agent {
	auto note_source_change(
		session& s,
		const std::filesystem::path& file,
		std::int64_t mtime
	) -> gse::id;

	auto note_written_file(
		session& s,
		const std::filesystem::path& file
	) -> void;

	auto working_on_files(
		const session& s
	) -> bool;

	auto attribute_change(
		data& d,
		const build_runner::source_changed& change
	) -> void;

	auto refresh_stale(
		data& d
	) -> void;

	auto unbuilt_label(
		const session& s
	) -> std::string;

	auto touch_owner(
		data& d,
		std::span<const std::filesystem::path> files
	) -> session*;

	auto blame_owner(
		data& d,
		const build_runner::build_error& error
	) -> session*;

	auto attribute_build_errors(
		data& d,
		const build_runner::build_finished& finished,
		channel_write<blame_offer> offers
	) -> void;

	auto dispatch_blame(
		data& d,
		std::uint32_t session_id
	) -> void;

	auto fix_unclaimed(
		data& d
	) -> void;

	auto blame_label(
		const session& s
	) -> std::string;

	auto blame_prompt(
		std::string_view lead,
		std::span<const blamed_error> errors
	) -> std::string;

	auto arm_retry(
		session& s
	) -> void;

	auto link_delay(
		const data& d
	) -> time;

	auto resume_waiting(
		data& d
	) -> void;

	auto service_link(
		data& d
	) -> void;

	auto link_label(
		const data& d,
		const session& s
	) -> std::string;
}
