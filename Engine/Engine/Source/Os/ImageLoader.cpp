module gse.os:image_loader_shared;

import std;

import :image_loader;

import gse.math;
import gse.core;

auto gse::image::data::size_bytes() const -> std::size_t {
	return size.x() * size.y() * channels;
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
		memcpy(pixels.data() + i * 4, pixel_data);
	}

	return {
		.size = vec2u(size),
		.channels = 4,
		.pixels = std::move(pixels),
	};
}
