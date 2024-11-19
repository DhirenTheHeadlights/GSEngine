#include "Editor.h"
#include "Engine.h"
#include "Game.h"

int main() {
	Engine::initialize(Game::initialize, Game::close);

	Editor::initialize();

	Engine::Window::addRenderingInterface(std::make_shared<Editor::RenderingInterface>());

	Engine::run(Game::update, Game::render);
}