#include "Engine.h"
#include "Game.h"
#include "ResourcePaths.h"

auto main() -> int {
	gse::set_imgui_enabled(true);
	gse::debug::set_imgui_save_file_path(GOONSQUAD_RESOURCES_PATH "imgui_state.ini");
	gse::initialize(game::initialize, game::close);
	gse::run(game::update, game::render);
}
