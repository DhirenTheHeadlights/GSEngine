import std;

import gse;
import ide;

auto main() -> int {
	gse::start(
		[](gse::engine& e) -> void {
			e.add_system<ide::config_system>();
			e.add_system<ide::editor_app>();
		},
		{
			.title = "GSEditor",
			.render_world = false,
			.custom_chrome = true,
			.scale_ui_with_resolution = false,
		}
	);
	return 0;
}
