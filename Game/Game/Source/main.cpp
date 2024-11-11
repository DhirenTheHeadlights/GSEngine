#include "Engine.h"
#include "Game.h"

int main() {
	Engine::Debug::setImguiSaveFilePath(RESOURCES_PATH "imgui_state.json");
	Engine::initialize(Game::initialize, Game::close);
	Engine::run(Game::update, Game::render);
	return 0;
}
