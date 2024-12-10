#pragma once
#include <vector>
#include "Graphics/3D/Mesh.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
	class BoundingBoxMesh : public Mesh {
	public:
		BoundingBoxMesh(const Vec3<Length>& lower, const Vec3<Length>& upper);

		void update();
	private:
		void updateGrid();
		static Vertex createVertex(const glm::vec3& position);

		Vec3<Length> lower;
		Vec3<Length> upper;
	};
}