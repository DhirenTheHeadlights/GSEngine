#pragma once
#include "Engine.h"

namespace Engine {
	class Box : public Object {
	public:
		Box(const Vec3<Length>& position, const Vec3<Length>& size) 
			: Object("Box"), position(position), size(size) {}

		void initialize() override;
		void update() override {}
		void render() override {}

	private:
		Vec3<Length> position;
		Vec3<Length> size;
	};
}