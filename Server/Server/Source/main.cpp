import gse;
import gse.server;

import gs;

auto main() -> int {
	gse::start<gse::server_app, gs::scene_loader>();
}