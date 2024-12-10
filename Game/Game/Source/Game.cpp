#include "Game.h"

#include <imgui.h>

#include "Arena.h"
#include "Engine.h"
#include "Player.h"
#include "SphereLight.h"

std::shared_ptr<Game::Arena> arena;
std::shared_ptr<Game::Player> player;
std::shared_ptr<gse::box> box;
std::shared_ptr<gse::box> box2;
std::shared_ptr<Game::SphereLight> sphere;
std::shared_ptr<Game::SphereLight> sphere2;
std::shared_ptr<gse::sphere> sphere3;

bool inputHandlingEnabled = true;

void Game::setInputHandlingFlag(const bool enabled) {
	inputHandlingEnabled = enabled;
}

bool Game::initialize() {
	arena  = std::make_shared<Arena>();
	player = std::make_shared<Player>();
	box    = std::make_shared<gse::box>(gse::Vec3<gse::Meters>(20.f, -400.f, 20.f), gse::Vec3<gse::Meters>(20.f, 20.f, 20.f));
	box2   = std::make_shared<gse::box>(gse::Vec3<gse::Meters>(-20.f, -400.f, 20.f), gse::Vec3<gse::Meters>(40.f, 40.f, 40.f));
	sphere = std::make_shared<SphereLight>(gse::Vec3<gse::Meters>(0.f, -300.f, 0.f), gse::meters(10.f));
	//sphere2 = std::make_shared<SphereLight>(Engine::Vec3<Engine::Meters>(0.f, 0.f, 0.f), Engine::meters(1.f));
	sphere3 = std::make_shared<gse::sphere>(gse::Vec3<gse::Meters>(0.f, -400.f, 200.f), gse::meters(10.f));

	const auto scene1 = std::make_shared<gse::scene>();

	scene1->addObject(arena);
	scene1->addObject(player);
	scene1->addObject(box);
	scene1->addObject(box2);
	scene1->addObject(sphere);
	scene1->addObject(sphere2);
	scene1->addObject(sphere3);

	gse::scene_handler.addScene(scene1, "Scene1");
	gse::scene_handler.addScene(std::make_shared<gse::scene>(), "Scene2");

	gse::scene_handler.queueSceneTrigger(gse::grabID("Scene1").lock(), [] { return true; });

	return true;
}

bool Game::update() {
	if (inputHandlingEnabled) {
		gse::Window::setMouseVisible(gse::Input::getMouse().buttons[GLFW_MOUSE_BUTTON_MIDDLE].toggled || gse::Input::getKeyboard().keys[GLFW_KEY_N].toggled);

		if (gse::Input::getKeyboard().keys[GLFW_KEY_ENTER].pressed && gse::Input::getKeyboard().keys[GLFW_KEY_LEFT_ALT].held) {
			gse::Window::setFullScreen(!gse::Window::isFullScreen());
		}

		if (gse::Input::getKeyboard().keys[GLFW_KEY_ESCAPE].pressed) {
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
	gse::Debug::addImguiCallback([] {
		if (gse::scene_handler.getScene(gse::grabID("Scene1").lock())->getActive()) {
			ImGui::Begin("Game Data");

			ImGui::Text("FPS: %d", gse::MainClock::getFrameRate());

			ImGui::End();
		}
		});

	return true;
}

bool Game::close() {
	return true;
}
