#if defined(_DEBUG) && defined(_MSC_VER)
#include <crtdbg.h>
#endif

import std;

import gse;
import gs;

auto main() -> int {
#if defined(_DEBUG) && defined(_MSC_VER)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	gse::start([](gse::engine& e) {
		e.add_system<gs::client_system, gs::client_state>();
		e.add_system<gs::client_ui_system, gs::client_ui_state>();
		gs::world_loader_setup(e);
	});
}
