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
	) -> id;

	auto note_written_file(
		session& s,
		const std::filesystem::path& file
	) -> void;

	auto working_on_files(
		const session& s
	) -> bool;

	auto is_busy(
		const session& s
	) -> bool;

	auto mid_write(
		const session& s
	) -> bool;

	auto build_wait_for(
		const data& d,
		const session& s
	) -> build_wait;

	auto queued_for(
		const data& d,
		const session& s
	) -> const queued_build*;

	auto state_of(
		const data& d,
		const session& s
	) -> agent_state;

	auto style_of(
		agent_state state
	) -> state_style;

	auto status_label(
		const data& d,
		const session& s
	) -> std::string;

	auto exe_label(
		const session& s
	) -> std::string;

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

	auto build_patience() -> time;

	auto blocker_for(
		const data& d,
		const queued_build& queued
	) -> const session*;

	auto hold_for(
		const data& d,
		const queued_build& queued,
		bool building
	) -> build_hold_state;

	auto blocker_name(
		const data& d,
		std::uint32_t blocker
	) -> std::string_view;

	auto hold_message(
		const build_hold_state& hold,
		std::string_view blocker
	) -> std::string;

	auto hold_label(
		const data& d,
		const queued_build& queued
	) -> std::string;

	auto queue_label(
		const queued_build& queued
	) -> std::string;

	auto accept_requests(
		data& d
	) -> void;

	auto report_hold(
		queued_build& queued,
		const build_hold_state& hold,
		std::string_view blocker
	) -> void;

	auto request_of(
		const queued_build& queued
	) -> build_inbox::request;

	auto cancel_queued(
		data& d,
		std::string_view id
	) -> void;

	auto force_queued(
		data& d,
		std::string_view id
	) -> void;

	auto hand_off_builds(
		data& d,
		bool relaunching
	) -> void;

	auto accept_hibernations(
		data& d
	) -> void;

	auto wake_observers(
		data& d,
		const build_runner::build_finished& finished
	) -> void;

	auto hibernating_count(
		const data& d
	) -> std::size_t;

	auto poll_build_inbox(
		data& d,
		channel_write<build_runner::build_request> builds,
		bool building
	) -> void;

	auto publish_inbox_result(
		data& d,
		const build_runner::build_finished& finished
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