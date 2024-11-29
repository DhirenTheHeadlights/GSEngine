#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "SphereLight.h"

std::shared_ptr<Game::Arena> arena;
std::shared_ptr<Game::Player> player;
std::shared_ptr<Engine::Box> box;
std::shared_ptr<Game::SphereLight> sphere;

bool inputHandlingEnabled = true;

void Game::setInputHandlingFlag(const bool enabled) {
	inputHandlingEnabled = enabled;
}

bool Game::initialize() {
	arena  = std::make_shared<Arena>();
	player = std::make_shared<Player>();
	box    = std::make_shared<Engine::Box>(Engine::Vec3<Engine::Meters>(20.f, -400.f, 20.f), Engine::Vec3<Engine::Meters>(20.f, 20.f, 20.f));
	sphere = std::make_shared<SphereLight>(Engine::Vec3<Engine::Meters>(0.f, 0.f, 0.f), Engine::meters(10.f));

	const auto scene1 = std::make_shared<Engine::Scene>();

	scene1->addObject(arena);
	scene1->addObject(player);
	scene1->addObject(box);
	scene1->addObject(sphere);

	Engine::sceneHandler.addScene(scene1, "Scene1");
	Engine::sceneHandler.addScene(std::make_shared<Engine::Scene>(), "Scene2");

	Engine::sceneHandler.queueSceneTrigger(Engine::grabID("Scene1").lock(), [] { return true; });

	return true;
}

bool Game::update() {
	if (inputHandlingEnabled) {
		Engine::Window::setMouseVisible(Engine::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled || Engine::Input::getKeyboard().keys[GLFW_KEY_N].toggled);

		if (Engine::Input::getKeyboard().keys[GLFW_KEY_ENTER].pressed && Engine::Input::getKeyboard().keys[GLFW_KEY_LEFT_ALT].held) {
			Engine::Window::setFullScreen(!Engine::Window::isFullScreen());
		}

		if (Engine::Input::getKeyboard().keys[GLFW_KEY_ESCAPE].pressed) {
			if (close()) {
				std::cerr << "Game closed properly." << '\n';
				return false;
			}
			std::cerr << "Failed to close game properly." << '\n';
			return false;
		}
	}

	return true;
}

bool Game::render() {
	Engine::Debug::addImguiCallback([] {
		if (Engine::sceneHandler.getScene(Engine::grabID("Scene1").lock())->getActive()) {
			ImGui::Begin("Game Data");

			ImGui::Text("FPS: %d", Engine::MainClock::getFrameRate());

			ImGui::End();
		}
		});

	return true;
}

bool Game::close() {
	return true;
}
