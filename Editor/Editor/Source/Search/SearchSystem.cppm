export module gse.ide.search:search_system;

import std;
import gse;

import gse.ide.analysis;
import gse.ide.config;
import gse.ide.build;

import :index;

export namespace gse::ide::search_system {
	struct [[= gse::system_state<"Search">{}]] data {
		[[= gse::shared]] std::unique_ptr<search::index_state> index;
		std::uint32_t last_build_gen = 0;
	};

	[[= gse::system_init{}]]
	auto init(data& d) -> gse::async::task<>;

	[[= gse::system_frame{}]]
	auto frame(const gse::context& ctx, data& d) -> gse::async::task<>;
}

auto gse::ide::search_system::init(data& d) -> gse::async::task<> {
	d.index = std::make_unique<search::index_state>();
	d.index->workspace_root = gse::config::root_dir;

	if (const std::optional<std::filesystem::path> cc = analysis::diagnostics_runner::find_compile_commands(gse::config::root_dir)) {
		d.index->compile_commands = *cc;
	}
	std::error_code plugin_ec;
	if (std::filesystem::exists(gse::ide::config::token_plugin, plugin_ec)) {
		d.index->plugin_dll = gse::ide::config::token_plugin;
	}

	gse::task::post([index = d.index.get(), root = gse::config::root_dir] {
		search::build_files_and_content(*index, root);
	});
	gse::task::post([index = d.index.get()] {
		search::build_symbols(*index);
	});
	return {};
}

auto gse::ide::search_system::frame(const gse::context& ctx, data& d) -> gse::async::task<> {
	if (d.index) {
		for (const auto& req : ctx.read_channel<search::index_merge_request>()) {
			if (req.check) {
				d.index->merge_file_symbols(req.path, req.check->symbols, req.check->refs);
			}
		}

		const std::uint32_t gen = build_runner::game_build_generation();
		if (gen != d.last_build_gen && !d.index->building.load(std::memory_order_acquire)) {
			d.last_build_gen = gen;
			gse::task::post([index = d.index.get()] {
				search::build_symbols(*index);
			});
		}
	}
	return {};
}
