import std;

import gse;
import gse.gpu;
import gse.system_manifest;
import gse.ide;

auto main() -> int {
	gse::alloc::set_enabled(true);
	gse::trace::set_enabled(false);
	gse::ide::config::seed_editor_layout();

	gse::start(
		[](gse::engine& e) -> void {
			gse::register_systems<
				^^gse::ide::config_system,
				^^gse::ide::diagnostics_system,
				^^gse::ide::editor_app,
				^^gse::ide::workspace_system,
				^^gse::ide::terminal,
				^^gse::ide::build_runner,
				^^gse::ide::package_system,
				^^gse::ide::agent,
				^^gse::ide::viewport,
				^^gse::ide::search_system,
				^^gse::ide::git_system,
				^^gse::ide::profile_system,
				^^gse::ide::panel_query_system,
				^^gse::ide::format_query_system
			>(e);
		},
		{
			.title = gse::ide::project::current().valid
				? std::format("{} - GSEditor", gse::ide::project::current().name)
				: std::string("GSEditor"),
			.cadence = gse::loop_cadence::reactive,
			.simulate_world = false,
			.restart_on_device_lost = true,
			.restart_on_crash = true,
			.project_settings_path = gse::ide::config::project_settings(),
			.pins = {
				gse::settings::assign<^^gse::gpu::context::data::dark_background>(true),
				gse::settings::assign<^^gse::gpu::context::data::device_settings, ^^gse::gpu::device_settings::video_encode>(false),
				gse::settings::assign<^^gse::gui::data::reserve_top_bar>(true),
				gse::settings::assign<^^gse::gui::data::scale_with_resolution>(false),
				gse::settings::assign<^^gse::gui::data::file_path>(gse::ide::config::editor_layout()),
				gse::settings::assign<^^gse::window::data::native_frame>(true),
				gse::settings::assign<^^gse::window::data::mouse_visible>(true),
				gse::settings::assign<^^gse::window::data::launcher_size>(gse::ide::project::opened() ? gse::vec2i{ 0, 0 } : gse::vec2i{ 760, 420 }),
			},
		}
	);
	return 0;
}
