#pragma once

#include "glm/gtc/type_ptr.hpp"
#include "Graphics/Shader.h"

namespace gse {
	struct Material {
		Material(const std::string& vertexPath, const std::string& fragmentPath, const std::string& materialType) :
			materialType(materialType) {
			shader.createShaderProgram(vertexPath, fragmentPath);
		}

		void use(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) const {
			shader.use();
			shader.setMat4("view", view);
			shader.setMat4("projection", projection);
			shader.setMat4("model", model);
		}
		
		Shader shader;
		std::string materialType;
	};
}