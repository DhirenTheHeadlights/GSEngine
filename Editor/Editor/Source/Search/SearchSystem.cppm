export module gse.ide.search:search_system;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.config;
import gse.ide.build;

import :index;

export namespace gse::ide::search_system {
	struct [[= system_state<"Search">{}]] data {
		[[= stable_shared]] std::unique_ptr<search::index_state> index;
		file_watcher watcher;
		time last_index_change{};
		std::uint32_t last_build_gen = 0;
		bool symbols_dirty = false;
		bool build_was_running = false;
		std::vector<std::filesystem::path> watcher_changes;
	};

	[[= system_init{}]]
	auto init(data& d) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		data& d,
		shared_view<build_runner::data> build_d
	) -> async::task<>;
}

namespace gse::ide::search_system {
	auto is_symbol_source(const std::filesystem::path& path) -> bool;
}

auto gse::ide::search_system::is_symbol_source(const std::filesystem::path& path) -> bool {
	static constexpr std::string_view extensions[] = {
		".cpp", ".cppm", ".cc", ".cxx", ".ixx", ".c",
		".h", ".hpp", ".hh", ".hxx", ".inl"
	};
	std::string extension = path.extension().native_encoded_string();
	std::ranges::transform(extension, extension.begin(), [](const unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return std::ranges::find(extensions, extension) != std::ranges::end(extensions);
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

	search::start_symbol_worker(*d.index);
	search::request_symbol_build(*d.index);

	task::post([index = d.index.get()] {
		search::build_files_and_content(*index, index->roots);
	});
	for (const search::index_root& root : d.index->roots) {
		const std::filesystem::path path = root.path;
		d.watcher.watch_directory(
			path,
			[&d](const std::filesystem::path& changed) {
				d.watcher_changes.push_back(changed);
			},
			{},
			true,
			[path](const std::filesystem::path& candidate) {
				return search::is_indexed_path(path, candidate);
			}
		);
	}
	return {};
}

auto gse::ide::search_system::frame(const context& ctx, data& d, const shared_view<build_runner::data> build_d) -> async::task<> {
	if (d.index) {
		search::publish_file_build(*d.index);
		search::publish_symbol_build(*d.index);
		const time now = system_clock::now<time>();
		if (d.index->files.loaded.load(std::memory_order_acquire)) {
			d.watcher.poll();
		}
		std::vector<std::filesystem::path> watcher_changes;
		watcher_changes.swap(d.watcher_changes);
		for (const std::filesystem::path& path : watcher_changes) {
			search::update_file(*d.index, path);
			if (is_symbol_source(path)) {
				d.symbols_dirty = true;
				d.last_index_change = now;
			}
		}
		for (const auto& req : ctx.read_channel<search::index_file_update_request>()) {
			search::update_file(*d.index, req.path);
			if (is_symbol_source(req.path)) {
				d.symbols_dirty = true;
				d.last_index_change = system_clock::now<time>();
			}
		}
		for (const auto& req : ctx.read_channel<search::index_merge_request>()) {
			d.index->merge_file_symbols(req.path, req.symbols, req.refs, req.params);
		}

		const std::uint32_t gen = build_d.game_generation;
		if (gen != d.last_build_gen) {
			d.last_build_gen = gen;
			d.symbols_dirty = true;
			d.last_index_change = {};
		}
		if (build_d.building) {
			d.build_was_running = true;
		}
		else if (d.build_was_running) {
			d.build_was_running = false;
			d.symbols_dirty = true;
			d.last_index_change = {};
		}
		if (d.symbols_dirty && now - d.last_index_change > milliseconds(500.f) && !build_d.building && !d.index->building.load(std::memory_order_acquire)) {
			d.symbols_dirty = false;
			search::request_symbol_build(*d.index);
		}
	}
	return {};
}