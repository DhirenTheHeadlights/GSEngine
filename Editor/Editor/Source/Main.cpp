import std;

import gse;
import gse.system_manifest;
import gse.ide;

auto main() -> int {
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
				^^gse::ide::viewport,
				^^gse::ide::search_system,
				^^gse::ide::git_system
			>(e);
		},
		{
			.title = gse::ide::project::current().valid
				? std::format("{} - GSEditor", gse::ide::project::current().name)
				: std::string("GSEditor"),
			.render_world = false,
			.simulate_world = false,
			.custom_chrome = true,
			.scale_ui_with_resolution = false,
			.gui_layout_path = gse::ide::config::editor_layout(),
			.project_settings_path = gse::ide::config::project_settings(),
		}
	);
	return 0;
}
