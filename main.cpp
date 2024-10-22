#include "Game/Include/Game.h"
#include "Engine/Engine/Include/Engine.h"

int main() {
	Engine::initialize(Game::initialize, Game::close);
	Engine::run(Game::update, Game::render);
	return 0;
}
