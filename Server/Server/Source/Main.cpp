import gse;
import gse.server;

import gs;

auto main() -> int {
	gse::start(
		[](gse::engine& e) {
			gse::server_app_setup(e, gs::networked_components{});
			gs::world_loader_setup(e);
		},
		{
			.create_window = false,
			.render = false,
		}
	);
}
