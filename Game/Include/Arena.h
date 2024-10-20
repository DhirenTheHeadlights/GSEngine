#pragma once

#include "Engine/Engine.h"

namespace Game {
	class Arena final : public Engine::StaticObject {
	public:
		Arena() : StaticObject(Engine::idManager.generateID()) {}

		void initialize();
		void render();
	private:
		Engine::Units::Meters
			width   = 1000.f,
			height	= 1000.f,
			depth	= 1000.f;
	};
}