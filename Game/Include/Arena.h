#pragma once

#include <Engine.h>

namespace Game {
	class Arena final : public Engine::Object {
	public:
		Arena() : Object(Engine::idManager.generateID()) {}

		void initialize();
		void render();
	private:
		Engine::Units::Meters
			width   = 1000.f,
			height	= 1000.f,
			depth	= 1000.f;

		std::shared_ptr<Engine::RenderComponent> renderComponent = std::make_shared<Engine::RenderComponent>();
		std::shared_ptr<Engine::MeshComponent> meshComponent;
	};
}