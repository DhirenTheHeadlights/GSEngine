module gse.os:app_impl;

import std;

import :app;

import gse.log;

extern "C" auto gse_host_relaunch(const char* arguments, std::size_t length) -> void;

namespace gse::app {
	auto narrow(
		const std::wstring& text
	) -> std::string;
}

auto gse::app::narrow(const std::wstring& text) -> std::string {
	return std::filesystem::path(text).string();
}

auto gse::app::start_relaunch(const relaunch_plan& plan) -> void {
	if (!plan.self) {
		log::println(
			log::level::warning,
			log::category::general,
			"start_relaunch: '{}' cannot be launched from a web module; the request is dropped",
			plan.executable.generic_display_string()
		);
		return;
	}

	if (!plan.handles.empty()) {
		log::println(
			log::level::warning,
			log::category::general,
			"start_relaunch: {} handles cannot cross a web relaunch; relaunching without them",
			plan.handles.size()
		);
	}

	std::vector<std::string> arguments;
	if (const char* current = std::getenv("GSE_RUN_ARGS"); current != nullptr) {
		for (const auto token : std::string_view(current) | std::views::split(' ')) {
			const std::string argument(token.begin(), token.end());
			const bool dropped = std::ranges::any_of(plan.dropped_prefixes, [&](const std::wstring& prefix) {
				return argument.starts_with(narrow(prefix));
			});
			if (!argument.empty() && !dropped) {
				arguments.push_back(argument);
			}
		}
	}
	for (const std::wstring& pinned : plan.pinned_arguments) {
		if (std::string argument = narrow(pinned); std::ranges::find(arguments, argument) == arguments.end()) {
			arguments.push_back(std::move(argument));
		}
	}
	for (const std::wstring& handoff : plan.handoff_arguments) {
		arguments.push_back(narrow(handoff));
	}

	const std::string line = arguments | std::views::join_with(' ') | std::ranges::to<std::string>();
	gse_host_relaunch(line.data(), line.size());
}
