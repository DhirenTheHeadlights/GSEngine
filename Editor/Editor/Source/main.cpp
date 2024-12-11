#include "Editor.h"
#include "Engine.h"
#include "Game.h"

int main() {
	gse::initialize(game::initialize, game::close);

	Editor::initialize();

	gse::window::add_rendering_interface(std::make_shared<Editor::RenderingInterface>());

	gse::run(game::update, game::render);
}