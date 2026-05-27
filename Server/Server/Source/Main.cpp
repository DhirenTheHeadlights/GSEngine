import gse;
import gse.server;

import gs;

auto main() -> int {
	gse::start(
		[](gse::engine& e) {
			gse::server_app_setup<gse::server_system_for<gs::networked_components>>(e);
			gs::world_loader_setup(e);
		},
		{
			.create_window = false,
			.render = false,
		}
	);
}
