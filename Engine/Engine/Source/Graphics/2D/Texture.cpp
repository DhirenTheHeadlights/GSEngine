#include "Graphics/2D/Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

gse::Texture::Texture(const std::string& filepath) : filepath(filepath) {
	loadFromFile(filepath);
}

gse::Texture::~Texture() {
	glDeleteTextures(1, &textureID);
}

void gse::Texture::loadFromFile(const std::string& filepath) {
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(filepath.c_str(), &dimensions.x, &dimensions.y, &channels, 0);

	if (!data) {
		std::cerr << "Failed to load texture: " << filepath << '\n';
		return;
	}

	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);

	GLenum format = GL_RGB;
	if (channels == 4) {
		format = GL_RGBA;
	}
	else if (channels == 1) {
		format = GL_RED;
	}

	glTexImage2D(GL_TEXTURE_2D, 0, format, dimensions.x, dimensions.y, 0, format, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);

	stbi_image_free(data);
}

void gse::Texture::bind(const unsigned int unit) const {
	glActiveTexture(GL_TEXTURE0 + unit);
	glBindTexture(GL_TEXTURE_2D, textureID);
}

void gse::Texture::unbind() const {
	glBindTexture(GL_TEXTURE_2D, 0);
}

void gse::Texture::setWrapping(const GLenum wrapS, const GLenum wrapT) {
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapT);
	glBindTexture(GL_TEXTURE_2D, 0);
}

void gse::Texture::setFiltering(const GLenum minFilter, const GLenum magFilter) {
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilter);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magFilter);
	glBindTexture(GL_TEXTURE_2D, 0);
}