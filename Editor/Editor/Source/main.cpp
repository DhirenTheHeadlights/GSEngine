#include "Editor.h"
#include "Engine.h"
#include "Game.h"

int main() {
	gse::initialize(Game::initialize, Game::close);

	Editor::initialize();

	gse::window::add_rendering_interface(std::make_shared<Editor::RenderingInterface>());

	gse::run(Game::update, Game::render);
}