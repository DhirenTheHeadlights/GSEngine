export module gse.ide.agent:system;

import std;
import gse;

import gse.ide.build;
import gse.ide.navigation;

import :model;

export namespace gse::ide::agent {
	[[= system_run<>{}]]
	auto run(
		context& ctx,
		data& d,
		channel_read<start_request, dispatch_request, gui::context_menu_result, build_runner::build_finished, build_runner::source_changed> requests_in,
		channel_write<gui::menu_content, jump_to_request, set_cursor_shape_request, blame_offer> events_out,
		shared_view<asset::data> assets_d
	) -> async::task<>;

	[[= system_shutdown{}]]
	auto shutdown(
		data& d
	) -> void;
}
