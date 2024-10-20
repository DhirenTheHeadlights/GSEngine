#pragma once

#include "Engine/Include/Graphics/RenderComponent.h"
#include "Engine/Include/Physics/Vector/Vec3.h"

namespace Engine {
	class BoundingBoxRenderComponent : public RenderComponent {
	public:
		BoundingBoxRenderComponent(const Vec3<Length>& lower, const Vec3<Length>& upper)
			: lower(lower), upper(upper) {}
		BoundingBoxRenderComponent(const BoundingBoxRenderComponent&) = default;
		BoundingBoxRenderComponent(BoundingBoxRenderComponent&&) noexcept;

		void update(bool moving);
	private:
		void updateGrid();
		void initialize(bool moving);

		std::vector<float> vertices;

		Vec3<Length> lower;
		Vec3<Length> upper;

		bool isInitialized = false;
	};
}