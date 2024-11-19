#include "Engine.h"
#include "Game.h"
#include "ResourcePaths.h"

int main() {
	Engine::Debug::setImguiSaveFilePath(GOONSQUAD_RESOURCES_PATH "imgui_state.json");
	Engine::initialize(Game::initialize, Game::close);
	Engine::run(Game::update, Game::render);
	return 0;
}
