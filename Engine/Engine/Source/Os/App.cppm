export module gse.os:app;

import std;

export namespace gse::app {
	auto relaunch_on_exit(
		std::filesystem::path executable,
		std::filesystem::path working_dir = {},
		std::vector<std::filesystem::path> arguments = {}
	) -> void;

	auto relaunch_self_on_exit() -> void;

	auto relaunch_pending() -> bool;

	auto pin_relaunch_argument(
		std::wstring argument
	) -> void;

	auto add_relaunch_handoff(
		std::span<void* const> handles,
		std::wstring argument
	) -> void;

	auto drop_relaunch_arguments(
		std::wstring prefix
	) -> void;

	auto run_pending_relaunch() -> void;
}

namespace gse::app {
	struct relaunch_plan {
		std::filesystem::path executable;
		std::filesystem::path working_dir;
		std::vector<std::filesystem::path> arguments;
		std::vector<void*> handles;
		std::vector<std::wstring> handoff_arguments;
		std::vector<std::wstring> dropped_prefixes;
		std::vector<std::wstring> pinned_arguments;
		bool self = false;
	};

	auto start_relaunch(
		const relaunch_plan& plan
	) -> void;

	std::mutex relaunch_mutex;
	bool relaunch_queued = false;
	bool relaunch_self = false;
	std::filesystem::path relaunch_executable;
	std::filesystem::path relaunch_working_dir;
	std::vector<std::filesystem::path> relaunch_arguments;
	std::vector<void*> relaunch_handles;
	std::vector<std::wstring> relaunch_handoff_arguments;
	std::vector<std::wstring> relaunch_dropped_prefixes;
	std::vector<std::wstring> relaunch_pinned_arguments;
}

auto gse::app::relaunch_on_exit(std::filesystem::path executable, std::filesystem::path working_dir, std::vector<std::filesystem::path> arguments) -> void {
	std::lock_guard _(relaunch_mutex);
	relaunch_executable = std::move(executable);
	relaunch_working_dir = std::move(working_dir);
	relaunch_arguments = std::move(arguments);
	relaunch_handles.clear();
	relaunch_handoff_arguments.clear();
	relaunch_dropped_prefixes.clear();
	relaunch_self = false;
	relaunch_queued = true;
}

auto gse::app::relaunch_self_on_exit() -> void {
	std::lock_guard _(relaunch_mutex);
	relaunch_executable.clear();
	relaunch_working_dir.clear();
	relaunch_arguments.clear();
	relaunch_handles.clear();
	relaunch_handoff_arguments.clear();
	relaunch_dropped_prefixes.clear();
	relaunch_self = true;
	relaunch_queued = true;
}

auto gse::app::relaunch_pending() -> bool {
	std::lock_guard _(relaunch_mutex);
	return relaunch_queued;
}

auto gse::app::pin_relaunch_argument(std::wstring argument) -> void {
	std::lock_guard _(relaunch_mutex);
	if (std::ranges::find(relaunch_pinned_arguments, argument) == relaunch_pinned_arguments.end()) {
		relaunch_pinned_arguments.push_back(std::move(argument));
	}
}

auto gse::app::add_relaunch_handoff(const std::span<void* const> handles, std::wstring argument) -> void {
	std::lock_guard _(relaunch_mutex);
	if (!relaunch_queued) {
		return;
	}
	relaunch_handles.insert(relaunch_handles.end(), handles.begin(), handles.end());
	relaunch_handoff_arguments.push_back(std::move(argument));
}

auto gse::app::drop_relaunch_arguments(std::wstring prefix) -> void {
	std::lock_guard _(relaunch_mutex);
	if (!relaunch_queued || prefix.empty()) {
		return;
	}
	if (std::ranges::find(relaunch_dropped_prefixes, prefix) == relaunch_dropped_prefixes.end()) {
		relaunch_dropped_prefixes.push_back(std::move(prefix));
	}
}

auto gse::app::run_pending_relaunch() -> void {
	relaunch_plan plan;
	{
		std::lock_guard _(relaunch_mutex);
		if (!relaunch_queued) {
			return;
		}
		relaunch_queued = false;
		plan.self = std::exchange(relaunch_self, false);
		plan.executable = std::move(relaunch_executable);
		plan.working_dir = std::move(relaunch_working_dir);
		plan.arguments = std::move(relaunch_arguments);
		plan.handles = std::move(relaunch_handles);
		plan.handoff_arguments = std::move(relaunch_handoff_arguments);
		plan.dropped_prefixes = std::move(relaunch_dropped_prefixes);
		plan.pinned_arguments = relaunch_pinned_arguments;
	}

	start_relaunch(plan);
}
