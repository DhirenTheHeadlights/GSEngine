#pragma once

#include "Graphics/Shader.h"

enum class MaterialType {
	Solid,
	Transparent
};

namespace Engine {
	struct Material {
		Material(const std::string& vertexPath, const std::string& fragmentPath, MaterialType materialType) :
			shader(vertexPath, fragmentPath), materialType(materialType) {}
		
		Shader shader;
		MaterialType materialType;
	};
}