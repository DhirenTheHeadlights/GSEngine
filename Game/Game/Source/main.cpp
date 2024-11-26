#include "Engine.h"
#include "Game.h"
#include "ResourcePaths.h"

int main() {
	Engine::setImguiEnabled(true);
	Engine::Debug::setImguiSaveFilePath(GOONSQUAD_RESOURCES_PATH "imgui_state.ini");
	Engine::initialize(Game::initialize, Game::close);
	Engine::run(Game::update, Game::render);
	return 0;
}
