module;

#include <glad/glad.h>
#include "stb_image.h"

export module gse.graphics.texture_loader;

import std;

import gse.core.id;
import gse.platform.perma_assert;

export namespace gse::texture_loader {
	auto load_texture(const std::filesystem::path& path, bool gamma_correction) -> std::uint32_t;
	auto get_texture_by_path(const std::filesystem::path& texture_path) -> std::uint32_t;
	auto get_texture_by_id(id* texture_id) -> std::uint32_t;
}

struct texture;

std::unordered_map<gse::id*, texture> g_textures;

struct texture : gse::identifiable {
	texture(const std::string& tag, const std::uint32_t texture_id) : identifiable(tag), texture_id(texture_id) {}
	std::uint32_t texture_id;
};

auto gse::texture_loader::load_texture(const std::filesystem::path& path, const bool gamma_correction) -> std::uint32_t {
	for (const auto& [id, gl_texture] : g_textures) {
		if (id->tag() == path) {
			return gl_texture.texture_id; // Already loaded
		}
	}

	unsigned int texture_id;
	glGenTextures(1, &texture_id);

	int width = 0, height = 0, number_components = 0;
	if (unsigned char* data = stbi_load(path.string().c_str(), &width, &height, &number_components, 0)) {
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

	texture new_texture(path.string(), texture_id);

	g_textures.insert({ new_texture.get_id(), std::move(new_texture) });

	return texture_id;
}

auto gse::texture_loader::get_texture_by_path(const std::filesystem::path& texture_path) -> std::uint32_t {
	const auto it = std::ranges::find_if(g_textures, [&texture_path](const auto& texture) { return texture.first->tag() == texture_path; });
	if (it != g_textures.end()) {
		return it->second.texture_id;
	}
	std::cerr << "Grabbing non-existent texture; Loading in as a new texture without gamma correction... \n";
	return load_texture(texture_path, false);
}

auto gse::texture_loader::get_texture_by_id(id* texture_id) -> std::uint32_t {
	const auto it = g_textures.find(texture_id);
	perma_assert(it != g_textures.end(), "Grabbing non-existent texture by id, could not find.");
	return it->second.texture_id;
}