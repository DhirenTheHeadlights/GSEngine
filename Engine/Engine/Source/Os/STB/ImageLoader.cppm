export module gse.os:image_loader;

import std;

import gse.assert;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

export namespace gse::image {
	struct data {
		std::filesystem::path path;
		vec2u size;
		std::uint32_t channels = 0;
		std::vector<std::byte> pixels;

		auto size_bytes(
		) const -> std::size_t;
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

auto gse::image::data::size_bytes() const -> std::size_t {
	return size.x() * size.y() * channels;
}
