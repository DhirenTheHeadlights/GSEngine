import gse;
import gse.server;

import gs;

auto main() -> int {
	gse::start<gse::server_app, gs::world_loader>(gse::flags::none);
}