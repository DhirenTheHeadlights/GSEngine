#pragma once
#include "Engine.h"

namespace gse {
	class box : public object {
	public:
		box(const Vec3<Length>& position, const Vec3<Length>& size) 
			: object("Box"), position(position), size(size) {}

		void initialize() override;
		void update() override {}
		void render() override {}

	private:
		Vec3<Length> position;
		Vec3<Length> size;
	};
}