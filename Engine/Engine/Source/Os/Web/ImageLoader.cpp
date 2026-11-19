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

auto gse::image::write_png(const std::filesystem::path& path, const std::uint32_t width, const std::uint32_t height, const std::uint32_t channels, const void* pixels) -> bool {
	constexpr std::array<std::uint8_t, 5> color_types{ 0, 0, 4, 2, 6 };
	if (channels == 0 || channels > 4) {
		return false;
	}

	const auto stride = static_cast<std::size_t>(width) * channels;
	std::vector<std::uint8_t> raw;
	raw.reserve((stride + 1) * height);
	const auto* source = static_cast<const std::uint8_t*>(pixels);
	for (std::uint32_t row = 0; row < height; ++row) {
		raw.push_back(0);
		raw.insert(raw.end(), source + row * stride, source + (row + 1) * stride);
	}

	std::vector<std::uint8_t> zlib{ 0x78, 0x01 };
	constexpr std::size_t block_limit = 0xffff;
	std::size_t at = 0;
	do {
		const auto length = static_cast<std::uint16_t>(std::min(block_limit, raw.size() - at));
		const auto inverse = static_cast<std::uint16_t>(~length);
		zlib.push_back(at + length >= raw.size() ? 1 : 0);
		zlib.push_back(static_cast<std::uint8_t>(length));
		zlib.push_back(static_cast<std::uint8_t>(length >> 8));
		zlib.push_back(static_cast<std::uint8_t>(inverse));
		zlib.push_back(static_cast<std::uint8_t>(inverse >> 8));
		zlib.insert(zlib.end(), raw.begin() + at, raw.begin() + at + length);
		at += length;
	} while (at < raw.size());
	std::uint32_t adler_a = 1;
	std::uint32_t adler_b = 0;
	for (const auto byte : raw) {
		adler_a = (adler_a + byte) % 65521;
		adler_b = (adler_b + adler_a) % 65521;
	}
	const std::uint32_t adler = adler_b << 16 | adler_a;
	for (int shift = 24; shift >= 0; shift -= 8) {
		zlib.push_back(static_cast<std::uint8_t>(adler >> shift));
	}

	std::array<std::uint32_t, 256> crc_table{};
	for (std::uint32_t n = 0; n < 256; ++n) {
		std::uint32_t c = n;
		for (int k = 0; k < 8; ++k) {
			c = c & 1 ? 0xedb88320u ^ c >> 1 : c >> 1;
		}
		crc_table[n] = c;
	}

	std::vector<std::uint8_t> png{ 0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a };
	const auto put_u32 = [&png](const std::uint32_t value) {
		for (int shift = 24; shift >= 0; shift -= 8) {
			png.push_back(static_cast<std::uint8_t>(value >> shift));
		}
	};
	const auto chunk = [&](const std::string_view type, const std::span<const std::uint8_t> body) {
		put_u32(static_cast<std::uint32_t>(body.size()));
		const auto start = png.size();
		png.insert(png.end(), type.begin(), type.end());
		png.insert(png.end(), body.begin(), body.end());
		std::uint32_t crc = 0xffffffffu;
		for (auto i = start; i < png.size(); ++i) {
			crc = crc_table[(crc ^ png[i]) & 0xff] ^ crc >> 8;
		}
		put_u32(crc ^ 0xffffffffu);
	};

	std::array<std::uint8_t, 13> header{
		static_cast<std::uint8_t>(width >> 24), static_cast<std::uint8_t>(width >> 16), static_cast<std::uint8_t>(width >> 8), static_cast<std::uint8_t>(width),
		static_cast<std::uint8_t>(height >> 24), static_cast<std::uint8_t>(height >> 16), static_cast<std::uint8_t>(height >> 8), static_cast<std::uint8_t>(height),
		8, color_types[channels], 0, 0, 0
	};
	chunk("IHDR", header);
	chunk("IDAT", zlib);
	chunk("IEND", {});

	std::ofstream file(path, std::ios::binary);
	if (!file) {
		return false;
	}
	file.write(reinterpret_cast<const char*>(png.data()), static_cast<std::streamsize>(png.size()));
	return static_cast<bool>(file);
}
