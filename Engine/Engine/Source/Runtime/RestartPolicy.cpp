module gse.runtime:restart_policy_impl;

import gse.log;
import gse.math;
import gse.os;
import gse.stacktrace;
import gse.time;
import std;

import :bootstrap;
import :crash_report;
import :restart_policy;

namespace gse {
	constexpr std::wstring_view simulate_device_lost_option = L"--engine-simulate-device-lost";
	constexpr std::wstring_view restart_streak_option = L"--engine-restart-streak=";
	constexpr int max_restart_streak = 5;

	struct restart_policy {
		bool restart_on_crash = false;
		int streak = 0;
		clock since_launch;
	};

	restart_policy policy;

	auto command_line_has(
		std::wstring_view option
	) -> bool;

	auto restart_streak_from_command_line(
	) -> int;

	auto next_restart_streak(
		int streak,
		time uptime
	) -> int;

	auto hand_off_restart_streak(
		int streak
	) -> void;

	auto respond_to_crash(
		const crash_record& record
	) -> void;
}

auto gse::command_line_has(const std::wstring_view option) -> bool {
	const std::vector<std::wstring> arguments = command_line_arguments();
	return std::ranges::contains(arguments | std::views::drop(1), option);
}

auto gse::restart_streak_from_command_line() -> int {
	const std::vector<std::wstring> arguments = command_line_arguments();
	for (const std::wstring& argument : arguments | std::views::drop(1)) {
		if (argument.starts_with(restart_streak_option)) {
			return static_cast<int>(std::wcstol(argument.c_str() + restart_streak_option.size(), nullptr, 10));
		}
	}
	return 0;
}

auto gse::next_restart_streak(const int streak, const time uptime) -> int {
	if (uptime >= seconds(120.f)) {
		return 1;
	}
	return streak + 1;
}

auto gse::hand_off_restart_streak(const int streak) -> void {
	app::drop_relaunch_arguments(std::wstring(restart_streak_option));
	app::add_relaunch_handoff({}, std::format(L"{}{}", restart_streak_option, streak));
}

auto gse::respond_to_crash(const crash_record& record) -> void {
	const std::filesystem::path report = write_crash_report(record);
	if (!report.empty()) {
		log::println(log::level::error, log::category::general, "crash: {} - report written to {}", record.reason, report);
		log::flush();
	}

	if (policy.restart_on_crash && arm_restart(std::format("crash: {}", record.reason))) {
		app::run_pending_relaunch();
	}
}

auto gse::install_restart_policy(const bool restart_on_crash) -> void {
	policy = {
		.restart_on_crash = restart_on_crash,
		.streak = restart_streak_from_command_line(),
	};
	install_crash_responder(&respond_to_crash);
}

auto gse::arm_restart(const std::string_view cause) -> bool {
	const time uptime = policy.since_launch.elapsed();
	const int attempt = next_restart_streak(policy.streak, uptime);
	if (attempt > max_restart_streak) {
		log::println(log::level::error, log::category::runtime, "{} {::s} after launch, after {} consecutive restarts - not restarting again", cause, uptime, policy.streak);
		log::flush();
		return false;
	}

	log::println(log::level::error, log::category::runtime, "{} {::s} after launch - restarting ({}/{})", cause, uptime, attempt, max_restart_streak);
	log::flush();

	app::relaunch_self_on_exit();
	app::drop_relaunch_arguments(std::wstring(simulate_device_lost_option));
	hand_off_restart_streak(attempt);
	return true;
}

auto gse::device_lost_simulation_requested() -> bool {
	return command_line_has(simulate_device_lost_option);
}
