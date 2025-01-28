export module gse.graphics.material;

import std;
import glm;

import gse.graphics.shader;

export namespace gse {
	struct material {
		material(const std::string& vertex_path, const std::string& fragment_path, std::string material_type, const std::string& material_texture_path);

		auto use(const glm::mat4& view, const glm::mat4& projection, const glm::mat4& model) const -> void;
		
		shader shader;
		std::string material_type;
		std::uint32_t material_texture;
	};
}