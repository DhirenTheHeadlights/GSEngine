#pragma once

#include <Engine.h>

namespace Game {
	class Arena final : public Engine::Object {
	public:
		Arena() : Object(Engine::idHandler.generateID()) {}

		void initialize();
		void update() override;
		void render() override;
	private:
		Engine::Units::Meters
			width   = 1000.f,
			height	= 1000.f,
			depth	= 1000.f;
	};
}