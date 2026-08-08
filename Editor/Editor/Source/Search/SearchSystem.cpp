module gse.ide.search:search_system_impl;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.config;
import gse.ide.build;

import :index;
import :search_system;

namespace gse::ide::search_system {
	auto poll_watcher(
		data& d
	) -> void;
}

auto gse::ide::search_system::poll_watcher(data& d) -> void {
	const auto finish = make_scope_exit([&] {
		d.next_watcher_poll.store(system_clock::now<time>() + milliseconds(500.f), std::memory_order_release);
		d.watcher_polling.store(false, std::memory_order_release);
	});
	d.watcher.poll_now();
}

auto gse::ide::search_system::init(data& d) -> async::task<> {
	d.index = std::make_unique<search::index_state>();
	for (const config::browse_root& browse : config::browse_roots()) {
		if (!browse.analyzable) {
			continue;
		}
		d.index->roots.push_back({
			.path = browse.path,
			.name = browse.name,
			.compile_commands = browse.compile_commands,
			.is_project = browse.is_project
		});

		std::error_code compile_commands_ec;
		if (browse.compile_commands.empty() || !std::filesystem::exists(browse.compile_commands, compile_commands_ec)) {
			continue;
		}
		if (std::ranges::find(d.index->compile_commands, browse.compile_commands) == d.index->compile_commands.end()) {
			d.index->compile_commands.push_back(browse.compile_commands);
		}
	}
	std::error_code plugin_ec;
	if (std::filesystem::exists(config::token_plugin(), plugin_ec)) {
		d.index->plugin_dll = config::token_plugin();
	}
	else {
		log::println(
			log::level::error,
			log::category::general,
			"search: token plugin '{}' is missing - the symbol index, hover, go-to-definition and semantic highlighting will all be empty. CMake only builds it when the toolchain provides cc1plus.exe.a, which needs a GCC configured with --enable-plugin.",
			config::token_plugin().generic_display_string()
		);
	}

	for (const search::index_root& root : d.index->roots) {
		const std::filesystem::path path = root.path;
		d.watcher.watch_directory(
			path,
			[&d](const std::filesystem::path& changed) {
				std::lock_guard lock(d.watcher_changes_mutex);
				d.watcher_changes.push_back(changed);
			},
			{},
			true,
			[path](const std::filesystem::path& candidate) {
				return search::is_indexed_path(path, candidate);
			}
		);
	}

	search::start_symbol_worker(*d.index);
	search::request_symbol_build(*d.index);
	task::post([index = d.index.get()] {
		search::build_files_and_content(*index, index->roots);
	});
	return {};
}

auto gse::ide::search_system::frame(const context& ctx, data& d, const shared_view<build_runner::data> build_d) -> async::task<> {
	if (d.index) {
		d.index->cancel.store(build_runner::analysis_pause_requested(), std::memory_order_release);
		build_runner::report_analysis_busy(d.index->building.load(std::memory_order_acquire));

		search::publish_file_build(*d.index);
		search::publish_symbol_build(*d.index);
		const time now = system_clock::now<time>();
		if (d.index->files.loaded.load(std::memory_order_acquire) && now >= d.next_watcher_poll.load(std::memory_order_acquire) && !d.watcher_polling.exchange(true, std::memory_order_acq_rel)) {
			task::post([&d] {
				poll_watcher(d);
			});
		}
		std::vector<std::filesystem::path> watcher_changes;
		{
			std::lock_guard lock(d.watcher_changes_mutex);
			watcher_changes.swap(d.watcher_changes);
		}
		for (const std::filesystem::path& path : watcher_changes) {
			search::update_file(*d.index, path);
			if (search::is_symbol_source(path)) {
				d.symbols_dirty = true;
				d.last_index_change = now;
			}
		}
		for (const auto& req : ctx.read_channel<search::index_file_update_request>()) {
			search::update_file(*d.index, req.path);
			if (search::is_symbol_source(req.path)) {
				d.symbols_dirty = true;
				d.last_index_change = system_clock::now<time>();
			}
		}
		for (const auto& req : ctx.read_channel<search::index_merge_request>()) {
			d.index->merge_file_symbols(req.path, req.symbols, req.refs, req.params);
		}

		if (!ctx.read_channel<build_runner::build_finished>().empty()) {
			d.symbols_dirty = true;
			d.last_index_change = {};
		}
		if (d.symbols_dirty && now - d.last_index_change > milliseconds(500.f) && !build_d.building && !d.index->building.load(std::memory_order_acquire)) {
			d.symbols_dirty = false;
			search::request_symbol_build(*d.index);
		}
	}
	else {
		build_runner::report_analysis_busy(false);
	}
	return {};
}