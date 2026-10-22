module gse.os:image_loader_impl;

import std;

import :image_loader;

import gse.assert;
import gse.math;

auto gse::image::load(const std::filesystem::path& path) -> data {
	assert(false, "Image decoding is unsupported on this platform: {}", path.generic_display_string());
	return {};
}

auto gse::image::load_rgba(const std::filesystem::path& path) -> data {
	return load(path);
}

auto gse::image::load_cube_faces(const std::array<std::filesystem::path, 6>& paths) -> std::array<data, 6> {
	std::array<data, 6> faces;
	for (std::size_t i = 0; i < paths.size(); ++i) {
		faces[i] = load(paths[i]);
	}
	return faces;
}

auto gse::image::load_raw(const std::filesystem::path& path) -> data {
	return load(path);
}

auto gse::image::dimensions(const std::filesystem::path&) -> vec2u {
	return {};
}

auto gse::image::write_png(const std::filesystem::path&, std::uint32_t, std::uint32_t, std::uint32_t, const void*) -> bool {
	return false;
}
