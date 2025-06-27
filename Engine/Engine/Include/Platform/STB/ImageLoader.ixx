module;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#undef assert

export module gse.platform:image_loader;

import std;

import gse.assert;
import gse.physics.math;

export namespace gse::image {
	struct data {
		std::filesystem::path path;
		unitless::vec2u size;
		std::uint32_t channels = 0;
		std::vector<std::byte> pixels;

		auto size_bytes() const -> std::size_t {
			const auto combined = size.x * size.y * channels;
			assert(combined <= std::numeric_limits<std::size_t>::max(), "Image size exceeds maximum size_t value.");
			return combined;
		}
	};

	auto load(const std::filesystem::path& path) -> data;
	auto load_rgba(const std::filesystem::path& path) -> data;
	auto load_cube_faces(const std::array<std::filesystem::path, 6>& paths) -> std::array<data, 6>;
	auto load_raw(const std::filesystem::path& path, std::uint32_t channels) -> data;
	auto dimensions(const std::filesystem::path& path) -> unitless::vec2u;
}

auto gse::image::load(const std::filesystem::path& path) -> data {
    data img_data{
        .path = path
    };

    int w, h, c;
    auto* pixels = stbi_load(path.string().c_str(), &w, &h, &c, STBI_rgb_alpha);
    img_data.size = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    img_data.channels = 4;

    assert(pixels, std::format("Failed to load image: {}", path.string()));

    img_data.pixels.resize(img_data.size_bytes());
    std::memcpy(img_data.pixels.data(), pixels, img_data.size_bytes());

    stbi_image_free(pixels);

    return img_data;
}

auto gse::image::load_cube_faces(const std::array<std::filesystem::path, 6>& paths) -> std::array<data, 6> {
    std::array<data, 6> faces;

    faces[0] = load(paths[0]);
    auto required = faces[0].size;
    assert(required.x == required.y, std::format("Cube face must be square: {}", paths[0].string()));

    for (size_t i = 1; i < paths.size(); ++i) {
        faces[i] = load(paths[i]);
        assert(faces[i].size == required, std::format("All cube faces must match size {}×{}: {}", required.x, required.y, paths[i].string()));
    }

    return faces;
}

auto gse::image::load_raw(const std::filesystem::path& path, const std::uint32_t channels) -> data {
    data img_data{
        .path = path
    };

    int w, h, c;
    auto* pixels = stbi_load(path.string().c_str(), &w, &h, &c, 0);
    img_data.size = { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
    img_data.channels = static_cast<uint32_t>(c);

    assert(pixels, std::format("Failed to load image: {}", path.string()));

    img_data.pixels.resize(img_data.size_bytes());
    std::memcpy(img_data.pixels.data(), pixels, img_data.size_bytes());
    stbi_image_free(pixels);

    return img_data;
}

auto gse::image::dimensions(const std::filesystem::path& path) -> unitless::vec2u {
    int w, h, c;
    auto* pixels = stbi_load(path.string().c_str(), &w, &h, &c, 0);
    stbi_image_free(pixels);
    return { static_cast<uint32_t>(w), static_cast<uint32_t>(h) };
}