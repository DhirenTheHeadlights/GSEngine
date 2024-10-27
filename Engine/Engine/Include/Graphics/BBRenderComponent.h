#pragma once
#include <vector>
#include "Graphics/RenderComponent.h"
#include "Physics/Vector/Vec3.h"

namespace Engine {
	class BoundingBoxRenderComponent : public RenderComponent {
	public:
		BoundingBoxRenderComponent(const Vec3<Length>& lower, const Vec3<Length>& upper);
		BoundingBoxRenderComponent(const BoundingBoxRenderComponent&);
		BoundingBoxRenderComponent(BoundingBoxRenderComponent&&) noexcept;
		~BoundingBoxRenderComponent();

		void update(bool moving);
		void render() const;
	private:
		void updateGrid();
		void initialize(bool moving);

		std::vector<float> vertices;

		Vec3<Length> lower;
		Vec3<Length> upper;

		bool isInitialized = false;

		GLuint VAO;
		GLuint VBO;
	};
}