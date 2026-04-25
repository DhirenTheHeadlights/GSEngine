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
	gse::start<gs::client, gs::client_ui, gs::world_loader>();
}
