#pragma once
#include <vector>
#include "Graphics/3D/Mesh.h"
#include "Physics/Vector/Vec3.h"

namespace gse {
	class bounding_box_mesh final : public mesh {
	public:
		bounding_box_mesh(const vec3<length>& lower, const vec3<length>& upper);

		void update();
	private:
		void update_grid();
		static vertex create_vertex(const glm::vec3& position);

		vec3<length> m_lower;
		vec3<length> m_upper;
	};
}