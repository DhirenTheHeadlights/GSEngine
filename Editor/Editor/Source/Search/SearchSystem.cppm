export module gse.ide.search:search_system;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.config;
import gse.ide.build;

import :index;

export namespace gse::ide::search_system {
	struct [[= gse::system_state<"Search">{}]] data {
		[[= gse::stable_shared]] std::unique_ptr<search::index_state> index;
		gse::file_watcher watcher;
		gse::time last_index_change{};
		std::atomic<std::uint64_t> symbol_change_generation = 0;
		std::uint64_t last_symbol_change_generation = 0;
		std::uint32_t last_build_gen = 0;
		bool symbols_dirty = false;
		std::jthread watcher_thread;
	};

	[[= gse::system_init{}]]
	auto init(data& d) -> gse::async::task<>;

	[[= gse::system_frame{}]]
	auto frame(const gse::context& ctx, data& d) -> gse::async::task<>;
}

namespace gse::ide::search_system {
	auto is_symbol_source(const std::filesystem::path& path) -> bool {
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

	auto watcher_poll_loop(const std::stop_token& st, data* d) -> void {
		while (!st.stop_requested()) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			if (st.stop_requested()) {
				return;
			}
			if (d->index->files.loaded.load(std::memory_order_acquire)) {
				d->watcher.poll_now();
			}
		}
	}
}

auto gse::ide::search_system::init(data& d) -> gse::async::task<> {
	d.index = std::make_unique<search::index_state>();
	d.index->workspace_root = gse::config::root_dir;
	d.last_build_gen = build_runner::game_build_generation();

	if (const std::optional<std::filesystem::path> cc = analysis::diagnostics_runner::find_compile_commands(gse::config::root_dir)) {
		d.index->compile_commands = *cc;
	}
	std::error_code plugin_ec;
	if (std::filesystem::exists(gse::ide::config::token_plugin, plugin_ec)) {
		d.index->plugin_dll = gse::ide::config::token_plugin;
	}

	search::start_symbol_worker(*d.index);
	search::request_symbol_build(*d.index);

	gse::task::post([index = d.index.get(), root = gse::config::root_dir] {
		search::build_files_and_content(*index, root);
	});
	gse::task::post([&d, root = gse::config::root_dir] {
		const std::filesystem::path log_path = (root / "Engine" / "Resources" / "Misc" / "log.txt").lexically_normal();
		auto on_change = [&d, log_path](const std::filesystem::path& path) {
			if (path.lexically_normal() == log_path) {
				return;
			}
			search::update_file(*d.index, path);
			if (is_symbol_source(path)) {
				d.symbol_change_generation.fetch_add(1, std::memory_order_release);
			}
		};
		d.watcher.watch_directory(
			root,
			std::move(on_change),
			{},
			true,
			[root](const std::filesystem::path& path) {
				return search::is_indexed_path(root, path);
			}
		);
		d.watcher_thread = std::jthread(watcher_poll_loop, &d);
	});
	return {};
}

auto gse::ide::search_system::frame(const gse::context& ctx, data& d) -> gse::async::task<> {
	if (d.index) {
		const gse::time now = gse::system_clock::now<gse::time>();
		const std::uint64_t symbol_changes = d.symbol_change_generation.load(std::memory_order_acquire);
		if (symbol_changes != d.last_symbol_change_generation) {
			d.last_symbol_change_generation = symbol_changes;
			d.symbols_dirty = true;
			d.last_index_change = now;
		}
		for (const auto& req : ctx.read_channel<search::index_file_update_request>()) {
			search::update_file(*d.index, req.path);
			if (is_symbol_source(req.path)) {
				d.symbols_dirty = true;
				d.last_index_change = gse::system_clock::now<gse::time>();
			}
		}
		for (const auto& req : ctx.read_channel<search::index_merge_request>()) {
			if (req.check) {
				d.index->merge_file_symbols(req.path, req.check->symbols, req.check->refs);
			}
		}

		const std::uint32_t gen = build_runner::game_build_generation();
		if (gen != d.last_build_gen) {
			d.last_build_gen = gen;
			d.symbols_dirty = true;
			d.last_index_change = {};
		}
		if (d.symbols_dirty
			&& now - d.last_index_change > gse::milliseconds(500.f)
			&& !d.index->building.load(std::memory_order_acquire)) {
			d.symbols_dirty = false;
			search::request_symbol_build(*d.index);
		}
	}
	return {};
}
