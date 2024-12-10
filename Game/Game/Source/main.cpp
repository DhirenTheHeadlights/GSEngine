#include "Engine.h"
#include "Game.h"
#include "ResourcePaths.h"

int main() {
	gse::set_imgui_enabled(true);
	gse::Debug::setImguiSaveFilePath(GOONSQUAD_RESOURCES_PATH "imgui_state.ini");
	gse::initialize(Game::initialize, Game::close);
	gse::run(Game::update, Game::render);
	return 0;
}
