import std;

import gse;
import gs;

extern "C" const char* __asan_default_options() {
	return "detect_container_overflow=0";
}

auto main() -> int {
	gse::start<gs::client, gs::client_ui, gs::world_loader>();
}
