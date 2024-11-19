#pragma once
#include "Engine.h"

namespace Game {
	class Box : public Engine::Object {
	public:
		Box(const Engine::Vec3<Engine::Length>& position, const Engine::Vec3<Engine::Length>& size) 
			: Object("Box"), position(position), size(size) {}

		void initialize();
		void update() override;
		void render() override;

	private:
		Engine::Vec3<Engine::Length> position;
		Engine::Vec3<Engine::Length> size;
	};
}