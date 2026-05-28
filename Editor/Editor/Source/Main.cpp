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
		}
	);
	return 0;
}
