module;

#include <cstdio>

module gse.os;

import std;

import gse.assert;
import gse.math;
import gse.core;
import gse.libpng;
import gse.libjpeg;

namespace gse::image {
	enum class image_format {
		png,
		jpeg,
		unknown
	};

	struct read_result {
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t channels = 0;
		std::vector<std::byte> pixels;
		bool ok = false;
	};

	struct jpeg_error_mgr_ext {
		jpeg_error_mgr base;
		std::jmp_buf buf;
	};

	auto detect_format(
		const std::filesystem::path& path
	) -> image_format;

	auto read_png_file(
		const std::filesystem::path& path,
		bool force_rgba
	) -> read_result;

	auto read_jpeg_file(
		const std::filesystem::path& path,
		bool force_rgba
	) -> read_result;

	auto load_any(
		const std::filesystem::path& path,
		bool force_rgba
	) -> read_result;

	auto write_png_file(
		const std::filesystem::path& path,
		std::uint32_t width,
		std::uint32_t height,
		std::uint32_t channels,
		const void* pixels
	) -> bool;

	void jpeg_error_exit(
		j_common_ptr cinfo
	);
}

auto gse::image::data::size_bytes() const -> std::size_t {
	return size.x() * size.y() * channels;
}

auto gse::image::detect_format(const std::filesystem::path& path) -> image_format {
	std::FILE* f = nullptr;
	if (fopen_s(&f, path.string().c_str(), "rb") != 0) {
		return image_format::unknown;
	}
	unsigned char header[8] = {};
	const auto read = std::fread(header, 1, 8, f);
	std::fclose(f);
	if (read >= 8 && png_sig_cmp(header, 0, 8) == 0) {
		return image_format::png;
	}
	if (read >= 3 && header[0] == 0xFF && header[1] == 0xD8 && header[2] == 0xFF) {
		return image_format::jpeg;
	}
	return image_format::unknown;
}

auto gse::image::read_png_file(const std::filesystem::path& path, const bool force_rgba) -> read_result {
	read_result out;
	std::FILE* fp = nullptr;
	if (fopen_s(&fp, path.string().c_str(), "rb") != 0) {
		return out;
	}

	png_structp png = png_create_read_struct(png_libpng_ver_string, nullptr, nullptr, nullptr);
	if (!png) {
		std::fclose(fp);
		return out;
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_read_struct(&png, nullptr, nullptr);
		std::fclose(fp);
		return out;
	}

	bool failed = false;
	png_guarded(
		png,
		[&] {
			png_init_io(png, fp);
			png_read_info(png, info);

			const auto width = png_get_image_width(png, info);
			const auto height = png_get_image_height(png, info);
			auto color_type = png_get_color_type(png, info);
			const auto bit_depth = png_get_bit_depth(png, info);

			if (bit_depth == 16) {
				png_set_strip_16(png);
			}
			if (color_type == png_color_type_palette) {
				png_set_palette_to_rgb(png);
			}
			if (color_type == png_color_type_gray && bit_depth < 8) {
				png_set_expand_gray_1_2_4_to_8(png);
			}
			if (png_get_valid(png, info, png_info_trns)) {
				png_set_tRNS_to_alpha(png);
			}

			if (force_rgba) {
				if (color_type == png_color_type_rgb ||
					color_type == png_color_type_gray ||
					color_type == png_color_type_palette) {
					png_set_filler(png, 0xFF, png_filler_after);
				}
				if (color_type == png_color_type_gray || color_type == png_color_type_gray_alpha) {
					png_set_gray_to_rgb(png);
				}
			}

			png_read_update_info(png, info);

			const std::uint32_t channels = png_get_channels(png, info);
			const auto rowbytes = png_get_rowbytes(png, info);

			std::vector<std::byte> pixels(static_cast<std::size_t>(rowbytes) * height);
			std::vector<png_bytep> rows(height);
			for (std::uint32_t y = 0; y < height; ++y) {
				rows[y] = reinterpret_cast<png_bytep>(pixels.data() + static_cast<std::size_t>(y) * rowbytes);
			}
			png_read_image(png, rows.data());

			out.width = width;
			out.height = height;
			out.channels = channels;
			out.pixels = std::move(pixels);
			out.ok = true;
		},
		[&] {
			failed = true;
		}
	);

	png_destroy_read_struct(&png, &info, nullptr);
	std::fclose(fp);
	if (failed) {
		out = {};
	}
	return out;
}

void gse::image::jpeg_error_exit(j_common_ptr cinfo) {
	auto* err = reinterpret_cast<jpeg_error_mgr_ext*>(cinfo->err);
	std::longjmp(err->buf, 1);
}

auto gse::image::read_jpeg_file(const std::filesystem::path& path, const bool force_rgba) -> read_result {
	read_result out;
	std::FILE* fp = nullptr;
	if (fopen_s(&fp, path.string().c_str(), "rb") != 0) {
		return out;
	}

	jpeg_decompress_struct cinfo{};
	jpeg_error_mgr_ext err{};
	cinfo.err = jpeg_std_error(&err.base);
	err.base.error_exit = &jpeg_error_exit;

	bool failed = false;
	jpeg_guarded(
		err.buf,
		[&] {
			jpeg_create_decompress_wrap(&cinfo);
			jpeg_stdio_src(&cinfo, fp);
			jpeg_read_header(&cinfo, jpeg_true);

			cinfo.out_color_space = jpeg_color_space_rgb;
			jpeg_start_decompress(&cinfo);

			const std::uint32_t width = cinfo.output_width;
			const std::uint32_t height = cinfo.output_height;
			const std::uint32_t src_channels = cinfo.output_components;
			const std::uint32_t dst_channels = force_rgba ? 4u : src_channels;

			std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height * dst_channels);
			std::vector<unsigned char> row(static_cast<std::size_t>(width) * src_channels);

			while (cinfo.output_scanline < height) {
				unsigned char* row_ptr = row.data();
				jpeg_read_scanlines(&cinfo, &row_ptr, 1);
				const auto y = cinfo.output_scanline - 1;
				auto* dst = reinterpret_cast<unsigned char*>(pixels.data() + static_cast<std::size_t>(y) * width * dst_channels);
				if (force_rgba) {
					for (std::uint32_t x = 0; x < width; ++x) {
						dst[x * 4 + 0] = row[x * src_channels + 0];
						dst[x * 4 + 1] = row[x * src_channels + 1];
						dst[x * 4 + 2] = row[x * src_channels + 2];
						dst[x * 4 + 3] = 0xFF;
					}
				}
				else {
					std::memcpy(dst, row.data(), static_cast<std::size_t>(width) * src_channels);
				}
			}

			jpeg_finish_decompress(&cinfo);

			out.width = width;
			out.height = height;
			out.channels = dst_channels;
			out.pixels = std::move(pixels);
			out.ok = true;
		},
		[&] {
			failed = true;
		}
	);

	jpeg_destroy_decompress(&cinfo);
	std::fclose(fp);
	if (failed) {
		out = {};
	}
	return out;
}

auto gse::image::load_any(const std::filesystem::path& path, const bool force_rgba) -> read_result {
	switch (detect_format(path)) {
	case image_format::png:
		return read_png_file(path, force_rgba);
	case image_format::jpeg:
		return read_jpeg_file(path, force_rgba);
	case image_format::unknown:
		return {};
	}
	return {};
}

auto gse::image::write_png_file(
	const std::filesystem::path& path,
	const std::uint32_t width,
	const std::uint32_t height,
	const std::uint32_t channels,
	const void* pixels
) -> bool {
	std::FILE* fp = nullptr;
	if (fopen_s(&fp, path.string().c_str(), "wb") != 0) {
		return false;
	}

	png_structp png = png_create_write_struct(png_libpng_ver_string, nullptr, nullptr, nullptr);
	if (!png) {
		std::fclose(fp);
		return false;
	}
	png_infop info = png_create_info_struct(png);
	if (!info) {
		png_destroy_write_struct(&png, nullptr);
		std::fclose(fp);
		return false;
	}

	bool ok = false;
	png_guarded(
		png,
		[&] {
			png_init_io(png, fp);

			int color_type = png_color_type_rgba;
			switch (channels) {
			case 1:
				color_type = png_color_type_gray;
				break;
			case 2:
				color_type = png_color_type_gray_alpha;
				break;
			case 3:
				color_type = png_color_type_rgb;
				break;
			case 4:
				color_type = png_color_type_rgba;
				break;
			default:
				break;
			}

			png_set_IHDR(
				png,
				info,
				width,
				height,
				8,
				color_type,
				png_interlace_none,
				png_compression_type_default,
				png_filter_type_default
			);
			png_write_info(png, info);

			const auto* bytes = static_cast<const unsigned char*>(pixels);
			std::vector<png_bytep> rows(height);
			for (std::uint32_t y = 0; y < height; ++y) {
				rows[y] = const_cast<png_bytep>(bytes + static_cast<std::size_t>(y) * width * channels);
			}
			png_write_image(png, rows.data());
			png_write_end(png, nullptr);
			ok = true;
		},
		[&] {
			ok = false;
		}
	);

	png_destroy_write_struct(&png, &info);
	std::fclose(fp);
	return ok;
}

auto gse::image::load(const std::filesystem::path& path) -> data {
	auto result = load_any(path, true);
	assert(result.ok, std::source_location::current(), "Failed to load image: {}", path.string());

	return {
		.path = path,
		.size = { result.width, result.height },
		.channels = 4,
		.pixels = std::move(result.pixels),
	};
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
		.pixels = std::move(pixels),
	};
}

auto gse::image::load_rgba(const std::filesystem::path& path) -> data {
	return load(path);
}

auto gse::image::load_cube_faces(const std::array<std::filesystem::path, 6>& paths) -> std::array<data, 6> {
	std::array<data, 6> faces;

	faces[0] = load(paths[0]);
	const auto required = faces[0].size;
	assert(
		required.x() == required.y(),
		std::source_location::current(),
		"Cube face must be square: {}",
		paths[0].string()
	);

	for (std::size_t i = 1; i < paths.size(); ++i) {
		faces[i] = load(paths[i]);
		assert(
			faces[i].size == required,
			std::source_location::current(),
			"All cube faces must match size {}: {}",
			required,
			paths[i].string()
		);
	}

	return faces;
}

auto gse::image::load_raw(const std::filesystem::path& path) -> data {
	auto result = load_any(path, false);
	assert(result.ok, std::source_location::current(), "Failed to load image: {}", path.string());

	return {
		.path = path,
		.size = { result.width, result.height },
		.channels = result.channels,
		.pixels = std::move(result.pixels),
	};
}

auto gse::image::dimensions(const std::filesystem::path& path) -> vec2u {
	auto result = load_any(path, false);
	return { result.width, result.height };
}

auto gse::image::write_png(
	const std::filesystem::path& path,
	const std::uint32_t width,
	const std::uint32_t height,
	const std::uint32_t channels,
	const void* pixels
) -> bool {
	return write_png_file(path, width, height, channels, pixels);
}
