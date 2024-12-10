#include "Editor.h"
#include "Engine.h"
#include "Game.h"

int main() {
	gse::initialize(Game::initialize, Game::close);

	Editor::initialize();

	gse::Window::addRenderingInterface(std::make_shared<Editor::RenderingInterface>());

	gse::run(Game::update, Game::render);
}