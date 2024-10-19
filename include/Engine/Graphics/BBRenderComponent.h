#pragma once

#include "Engine/Graphics/RenderComponent.h"
#include "Engine/Physics/Vector/Vec3.h"

namespace Engine {
	class BoundingBoxRenderComponent : public RenderComponent {
	public:
		void initializeGrid(const Vec3<Length>& lower, const Vec3<Length>& upper);
		void updateGrid(const Vec3<Length>& lower, const Vec3<Length>& upper, bool isMoving);
		void render() const;
	private:
		std::vector<float> vertices;
		bool isInitialized = false;
	};
}