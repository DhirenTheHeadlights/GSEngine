#pragma once

#include <utility>

#include "glm/gtc/type_ptr.hpp"
#include "Graphics/Shader.h"

namespace gse {
	struct material {
		material(const std::string& vertex_path, const std::string& fragment_path, std::string material_type, const std::string& material_texture_path);

		void use(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) const;
		
		shader shader;
		std::string material_type;
		GLuint material_texture;
	};
}