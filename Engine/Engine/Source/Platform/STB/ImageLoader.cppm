module;

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#undef assert

export module gse.platform:image_loader;

import std;

import gse.assert;
import gse.math;
import gse.utility;

export namespace gse::image {
	struct data {
		std::filesystem::path path;
		vec2u size;
		std::uint32_t channels = 0;
		std::vector<std::byte> pixels;

		auto size_bytes() const -> std::size_t {
			const auto combined = size.x() * size.y() * channels;
			assert(
                combined <= std::numeric_limits<std::size_t>::max(), 
                std::source_location::current(),
                "Image size exceeds maximum size_t value."
            );
			return combined;
		}
	};

	auto load(
        const std::filesystem::path& path
    ) -> data;

    auto load(
        vec4f color, 
        vec2u size
    ) -> data;

	auto load_rgba(
        const std::filesystem::path& path
    ) -> data;

	auto load_cube_faces(
        const std::array<std::filesystem::path, 6>& paths
    ) -> std::array<data, 6>;

	auto load_raw(
		const std::filesystem::path& path
	) -> data;

	auto dimensions(
        const std::filesystem::path& path
    ) -> vec2u;

	auto write_png(
		const std::filesystem::path& path,
		std::uint32_t width,
		std::uint32_t height,
		std::uint32_t channels,
		const void* pixels
	) -> bool;
}

auto gse::image::load(const std::filesystem::path& path) -> data {
    data img_data{
        .path = path
    };

    int w, h, c;
    auto* pixels = stbi_load(path.string().c_str(), &w, &h, &c, STBI_rgb_alpha);
    img_data.size = { static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h) };
    img_data.channels = 4;

    assert(pixels, std::source_location::current(), "Failed to load image: {}", path.string());

    img_data.pixels.resize(img_data.size_bytes());
    gse::memcpy(img_data.pixels.data(), pixels, img_data.size_bytes());

    stbi_image_free(pixels);

    return img_data;
}

auto gse::image::load(const vec4f color, const vec2u size) -> data {
    std::array<std::byte, 4> pixel_data;
    pixel_data[0] = static_cast<std::byte>(color.x() * 255.0f);
    pixel_data[1] = static_cast<std::byte>(color.y() * 255.0f);
    pixel_data[2] = static_cast<std::byte>(color.z() * 255.0f);
    pixel_data[3] = static_cast<std::byte>(color.w() * 255.0f);

    const std::size_t total_pixels = static_cast<std::size_t>(size.x()) * size.y();
    std::vector<std::byte> pixels(total_pixels * 4);

    for (std::size_t i = 0; i < total_pixels; ++i) {
        gse::memcpy(pixels.data() + i * 4, pixel_data);
    }

    return {
        .size = vec2u(size),
        .channels = 4,
        .pixels = std::move(pixels)
    };
}

auto gse::image::load_cube_faces(const std::array<std::filesystem::path, 6>& paths) -> std::array<data, 6> {
    std::array<data, 6> faces;

    faces[0] = load(paths[0]);
    auto required = faces[0].size;
    assert(required.x() == required.y(), std::source_location::current(), "Cube face must be square: {}", paths[0].string());

    for (size_t i = 1; i < paths.size(); ++i) {
        faces[i] = load(paths[i]);
        assert(faces[i].size == required, std::source_location::current(), "All cube faces must match size {}: {}", required, paths[i].string());
    }

    return faces;
}

auto gse::image::load_raw(const std::filesystem::path& path) -> data {
    data img_data{
        .path = path
    };

    int w, h, c;
    auto* pixels = stbi_load(path.string().c_str(), &w, &h, &c, 0);
    img_data.size = { static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h) };
    img_data.channels = static_cast<std::uint32_t>(c);

    assert(pixels, std::source_location::current(), "Failed to load image: {}", path.string());

    img_data.pixels.resize(img_data.size_bytes());
    gse::memcpy(img_data.pixels.data(), pixels, img_data.size_bytes());
    stbi_image_free(pixels);

    return img_data;
}

auto gse::image::dimensions(const std::filesystem::path& path) -> vec2u {
    int w, h, c;
    auto* pixels = stbi_load(path.string().c_str(), &w, &h, &c, 0);
    stbi_image_free(pixels);
    return {
    	static_cast<std::uint32_t>(w), static_cast<std::uint32_t>(h)
    };
}

auto gse::image::write_png(
	const std::filesystem::path& path,
	const std::uint32_t width,
	const std::uint32_t height,
	const std::uint32_t channels,
	const void* pixels
) -> bool {
	return stbi_write_png(
		path.string().c_str(),
		static_cast<int>(width),
		static_cast<int>(height),
		static_cast<int>(channels),
		pixels,
		static_cast<int>(width * channels)
	) != 0;
}