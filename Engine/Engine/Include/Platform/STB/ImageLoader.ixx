export module gse.platform.stb.image_loader;

import std;
import gse.physics.math;
import gse.platform.assert;

export namespace gse::stb::image {
	struct data {
		unitless::vec2_t<std::uint32_t> size;
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
	auto dimensions(const std::filesystem::path& path) -> unitless::vec2_t<std::uint32_t>;
}