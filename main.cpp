#include "Engine/Engine/Include/Engine.h"
#include "Game/Include/Game.h"

int main() {
	Engine::initialize(Game::initialize, Game::close);
	Engine::run(Game::update, Game::render);
	return 0;
}
