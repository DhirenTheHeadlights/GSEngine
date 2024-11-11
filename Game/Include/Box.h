#pragma once
#include "Engine.h"

namespace Game {
	class Box : public Engine::Object {
	public:
		Box(const Engine::Vec3<Engine::Length>& dimensions) : Object(Engine::idHandler.generateID("Box")), dimensions(dimensions) {}
		~Box() = default;

		void initialize();
		void update() override;
		void render() override;

	private:
		Engine::Vec3<Engine::Length> dimensions;
	};
}