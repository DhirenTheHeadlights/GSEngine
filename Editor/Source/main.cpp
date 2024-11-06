#include "Editor.h"
#include "Engine.h"
#include "Game.h"

int main() {
	Engine::initialize(Game::initialize, Game::close);

	Engine::Window::addRenderingInterface(std::make_shared<Editor::RenderingInterface>());

	Engine::Window::setFullScreen(false);
	Game::setResizingEnabled(false);

	Engine::run(Game::update, Game::render);
}