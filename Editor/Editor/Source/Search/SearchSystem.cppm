export module gse.ide.search:search_system;

import gse.concurrency;
import gse.ecs;
import gse.fs;
import gse.ide.analysis;
import gse.ide.build;
import gse.ide.config;
import gse.math;
import gse.time;
import std;

import :index;

export namespace gse::ide::search_system {
	struct [[= system_state<"Search">{}]] data {
		[[= stable_shared]] std::unique_ptr<search::index_state> index;
		file_watcher watcher;
		time last_index_change{};
		time next_query_poll{};
		bool symbols_dirty = false;
		std::atomic<bool> watcher_polling = false;
		std::atomic<time> next_watcher_poll{};
		std::mutex watcher_changes_mutex;
		std::vector<std::filesystem::path> watcher_changes;
	};

	[[= system_init{}]]
	auto init(
		data& d
	) -> async::task<>;

	[[= system_frame{}]]
	auto frame(
		const context& ctx,
		data& d,
		channel_read<search::index_file_update_request, search::index_merge_request, build_runner::build_finished> requests_in,
		channel_write<build_runner::source_changed> events_out,
		shared_view<build_runner::data> build_d
	) -> async::task<>;
}