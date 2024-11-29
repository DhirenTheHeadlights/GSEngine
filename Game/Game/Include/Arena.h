#pragma once

#include "Engine.h"

namespace Game {
	class Arena final : public Engine::Object {
	public:
		Arena() : Object("Arena") {}

		void initialize() override;
		void update() override;
		void render() override;
	private:
		Engine::Length
			width   = Engine::meters(1000.f),
			height	= Engine::meters(100.f),
			depth	= Engine::meters(1000.f);
	};
}