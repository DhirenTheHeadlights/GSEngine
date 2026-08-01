import gse;
import gse.server;

import sandbox;

auto main() -> int {
	gse::start(
		[](gse::engine& e) {
			gse::server_app_setup(e);
			sandbox::world_loader_setup(e);
		},
		{
			.create_window = false,
			.render = false,
		}
	);
}