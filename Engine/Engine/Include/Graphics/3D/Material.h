#pragma once

#include <utility>

#include "glm/gtc/type_ptr.hpp"
#include "Graphics/Shader.h"

namespace gse {
	struct material {
		material(const std::string& vertex_path, const std::string& fragment_path, std::string material_type) :
			material_type(std::move(material_type)) {
			shader.create_shader_program(vertex_path, fragment_path);
		}

		void use(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) const {
			shader.use();
			shader.set_mat4("view", view);
			shader.set_mat4("projection", projection);
			shader.set_mat4("model", model);
		}
		
		shader shader;
		std::string material_type;
	};
}