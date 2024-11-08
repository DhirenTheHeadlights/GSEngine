#pragma once
#include <vector>
#include "Graphics/Mesh.h"
#include "Physics/Vector/Vec3.h"

namespace Engine {
	class BoundingBoxMesh : public Mesh {
	public:
		BoundingBoxMesh(const Vec3<Length>& lower, const Vec3<Length>& upper);

		void update();

		RenderQueueEntry getQueueEntry() const override {
			return {
				"SolidColor",
				VAO,
				GL_LINES,
				static_cast<GLsizei>(vertices.size() / 3),
				glm::mat4(1.0f),
				0,
				{ 1.0f, 1.0f, 1.0f }
			};
		}
	private:
		void updateGrid();
		void initialize();

		std::vector<float> bbverticies;

		Vec3<Length> lower;
		Vec3<Length> upper;

		bool isInitialized = false;
	};
}