#include "Editor.h"
#include "Engine.h"
#include "Game.h"

int main() {
	gse::initialize(game::initialize, game::close);

	editor::initialize();

	gse::window::add_rendering_interface(std::make_shared<editor::rendering_interface>());

	gse::run(game::update, game::render);
}