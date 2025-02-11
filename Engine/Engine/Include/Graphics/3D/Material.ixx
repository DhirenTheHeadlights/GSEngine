module;

#include <glad/glad.h>
#include "stb_image.h"

export module gse.graphics.material;

import std;

import gse.graphics.shader;
import gse.physics.math;

export namespace gse {
	struct material {
		material(const std::string& vertex_path, const std::string& fragment_path, std::string material_type, const std::string& material_texture_path);

		auto use(const mat4& view, const mat4& projection, const mat4& model) const -> void;
		
		shader shader;
		std::string material_type;
		std::uint32_t material_texture;
	};
	struct mtl_material {
		gse::unitless::vec3 ambient = gse::unitless::vec3(1.0f);
		gse::unitless::vec3 diffuse = gse::unitless::vec3(1.0f);
		gse::unitless::vec3 specular = gse::unitless::vec3(1.0f);
		gse::unitless::vec3 emission = gse::unitless::vec3(0.0f);
		float shininess = 0.0f;
		float optical_density = 1.0f;
		float transparency = 1.0f;
		int illumination_model = 0;

		uint32_t diffuse_texture = 0;
		uint32_t normal_texture = 0;
		uint32_t specular_texture = 0;
	};
}

auto load_texture(char const* path, const bool gamma_correction) -> unsigned int {
	unsigned int texture_id;
	glGenTextures(1, &texture_id);

	int width = 0, height = 0, number_components = 0;
	if (unsigned char* data = stbi_load(path, &width, &height, &number_components, 0)) {
		GLenum internal_format;
		GLenum data_format;
		if (number_components == 1) {
			internal_format = data_format = GL_RED;
		}
		else if (number_components == 3) {
			internal_format = gamma_correction ? GL_SRGB : GL_RGB;
			data_format = GL_RGB;
		}
		else if (number_components == 4) {
			internal_format = gamma_correction ? GL_SRGB_ALPHA : GL_RGBA;
			data_format = GL_RGBA;
		}
		else {
			std::cout << "Texture format not supported\n";
			stbi_image_free(data);
			return 0;
		}

		glBindTexture(GL_TEXTURE_2D, texture_id);
		glTexImage2D(GL_TEXTURE_2D, 0, internal_format, width, height, 0, data_format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		stbi_image_free(data);
	}
	else {
		std::cout << "Texture failed to load at path: " << path << '\n';
		stbi_image_free(data);
	}

	return texture_id;
}

gse::material::material(const std::string& vertex_path, const std::string& fragment_path, std::string material_type, const std::string& material_texture_path) :
	material_type(std::move(material_type)) {
	shader.create_shader_program(vertex_path, fragment_path);
	material_texture = !material_texture_path.empty() ? load_texture(material_texture_path.c_str(), true) : 0;
}

auto gse::material::use(const mat4& view, const mat4& projection, const mat4& model) const -> void {
	shader.use();

	shader.set_mat4("view", view);
	shader.set_mat4("projection", projection);
	shader.set_mat4("model", model);
	shader.set_int("material_texture", 0);

	shader.set_bool("useTexture", material_texture != 0);

	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, material_texture);
}