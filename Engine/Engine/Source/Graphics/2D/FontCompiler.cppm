export module gse.graphics:font_compiler;

import std;

import gse.os;
import gse.config;
import gse.assets;
import gse.gpu;
import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;
import gse.freetype;
import gse.msdfgen;

import :font;

export namespace gse {

	auto bake(
		const std::filesystem::path& src,
		font::baked& out
	) -> bool;
}

auto gse::bake(const std::filesystem::path& src, font::baked& out) -> bool {
	FT_Library ft_lib;
	if (FT_Init_FreeType(&ft_lib)) {
		log::println(log::level::error, log::category::assets, "Failed to initialize FreeType");
		return false;
	}

	FT_Face ft_face;
	if (FT_New_Face(ft_lib, src.string().c_str(), 0, &ft_face)) {
		log::println(log::level::error, log::category::assets, "Failed to load font face from '{}'", src.string());
		FT_Done_FreeType(ft_lib);
		return false;
	}

	msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
	msdfgen::FontHandle* font_handle = loadFont(ft_handle, src.string().c_str());
	if (!font_handle) {
		log::println(log::level::error, log::category::assets, "Failed to load font into msdfgen: {}", src.string());
		FT_Done_Face(ft_face);
		FT_Done_FreeType(ft_lib);
		msdfgen::deinitializeFreetype(ft_handle);
		return false;
	}

	msdfgen::FontMetrics font_metrics{};
	getFontMetrics(font_metrics, font_handle, msdfgen_consts::em_normalized);

	std::vector<char32_t> codepoints;
	codepoints.reserve(128);
	for (char32_t cp = 0x20; cp <= 0x7E; ++cp) {
		codepoints.push_back(cp);
	}
	codepoints.push_back(0x25B2);
	codepoints.push_back(0x25BC);

	const int glyph_count = static_cast<int>(codepoints.size());
	constexpr int cell = 160;
	constexpr int atlas_cols = 16;
	const int atlas_rows = (glyph_count + atlas_cols - 1) / atlas_cols;
	const int atlas_width = atlas_cols * cell;
	const int atlas_height = atlas_rows * cell;

	constexpr float pixel_range = 8.0f;
	constexpr double scale = 128.0;
	constexpr double pixel_range_em = pixel_range / scale;
	constexpr double cell_em = static_cast<double>(cell) / scale;

	msdfgen::ErrorCorrectionConfig error_correction;
	error_correction.mode = msdfgen_consts::edge_priority;
	error_correction.distanceCheckMode = msdfgen_consts::always_check_distance;

	constexpr std::uint32_t channels = 4;
	std::vector<std::byte> atlas_data(
		static_cast<std::size_t>(atlas_width) * atlas_height * channels,
		std::byte{ 0 }
	);
	std::unordered_map<char32_t, glyph> glyphs;

	const double units_per_em = std::max<double>(ft_face->units_per_EM, 1);

	int baked_count = 0;
	int skipped_count = 0;

	for (int glyph_index = 0; glyph_index < glyph_count; ++glyph_index) {
		const char32_t cp = codepoints[glyph_index];
		const int col = glyph_index % atlas_cols;
		const int row = glyph_index / atlas_cols;
		const int cell_x = col * cell;
		const int cell_y = row * cell;

		FT_Load_Glyph(ft_face, FT_Get_Char_Index(ft_face, static_cast<FT_UInt>(cp)), freetype_load_no_scale);
		const FT_GlyphSlot ft_glyph = ft_face->glyph;
		const float x_advance_em = static_cast<float>(ft_glyph->advance.x / units_per_em);
		const auto ft_index = FT_Get_Char_Index(ft_face, static_cast<FT_UInt>(cp));

		msdfgen::Shape shape;
		const bool loaded =
			msdfgen::loadGlyph(
				shape,
				font_handle,
				static_cast<msdfgen::unicode_t>(cp),
				msdfgen_consts::em_normalized
			);

		bool has_geometry = loaded && !shape.contours.empty();
		if (has_geometry) {
			shape.normalize();
			shape.orientContours();
		}

		const auto bounds = shape.getBounds();
		const double shape_w = bounds.r - bounds.l;
		const double shape_h = bounds.t - bounds.b;
		const bool valid_bounds = has_geometry && shape_w > 0.0 && shape_h > 0.0;

		if (!valid_bounds) {
			if (cp != U' ') {
				log::println(
					log::level::warning,
					log::category::assets,
					"font bake [{}]: codepoint U+{:04X} has no usable geometry (loaded={}, contours={}, w={}, h={})",
					src.filename().string(),
					static_cast<unsigned>(cp),
					loaded,
					shape.contours.size(),
					shape_w,
					shape_h
				);
				++skipped_count;
			}

			glyphs[cp] = glyph(
				glyph::info{
					.ft_glyph_index = ft_index,
					.atlas_uv = {},
					.plane_bounds = {},
					.x_advance = x_advance_em,
				}
			);
			continue;
		}

		const double padded_w = shape_w + 2.0 * pixel_range_em;
		const double padded_h = shape_h + 2.0 * pixel_range_em;

		if (padded_w > cell_em || padded_h > cell_em) {
			log::println(
				log::level::warning,
				log::category::assets,
				"font bake [{}]: codepoint U+{:04X} padded extent ({}x{} em) exceeds cell ({} em); clipping may occur",
				src.filename().string(),
				static_cast<unsigned>(cp),
				padded_w,
				padded_h,
				cell_em
			);
		}

		const double translate_x = cell_em * 0.5 - (bounds.l + shape_w * 0.5);
		const double translate_y = cell_em * 0.5 - (bounds.b + shape_h * 0.5);

		msdfgen::edgeColoringByDistance(shape, 3.0);
		msdfgen::Bitmap<float, 4> mtsdf(cell, cell);
		msdfgen::generateMTSDF(
			mtsdf,
			shape,
			msdfgen::Range(pixel_range_em),
			msdfgen::Vector2{ scale, scale },
			msdfgen::Vector2{ translate_x, translate_y },
			error_correction
		);

		for (int y = 0; y < cell; ++y) {
			for (int x = 0; x < cell; ++x) {
				const int atlas_x = cell_x + x;
				const int atlas_y = cell_y + (cell - 1 - y);
				const std::size_t idx = (static_cast<std::size_t>(atlas_y) * atlas_width + atlas_x) * channels;
				const auto* px = mtsdf(x, y);
				atlas_data[idx + 0] = static_cast<std::byte>(std::clamp(px[0], 0.f, 1.f) * 255.f);
				atlas_data[idx + 1] = static_cast<std::byte>(std::clamp(px[1], 0.f, 1.f) * 255.f);
				atlas_data[idx + 2] = static_cast<std::byte>(std::clamp(px[2], 0.f, 1.f) * 255.f);
				atlas_data[idx + 3] = static_cast<std::byte>(std::clamp(px[3], 0.f, 1.f) * 255.f);
			}
		}

		const double quad_cell_x = (cell_em - padded_w) * 0.5 * scale;
		const double quad_cell_y_top = (cell_em - padded_h) * 0.5 * scale;
		const double quad_w_atlas = padded_w * scale;
		const double quad_h_atlas = padded_h * scale;

		const vec4f atlas_uv{
			static_cast<float>((cell_x + quad_cell_x) / atlas_width),
			static_cast<float>((cell_y + quad_cell_y_top) / atlas_height),
			static_cast<float>(quad_w_atlas / atlas_width),
			static_cast<float>(quad_h_atlas / atlas_height),
		};

		const vec4f plane_bounds{
			static_cast<float>(bounds.l - pixel_range_em),
			static_cast<float>(bounds.b - pixel_range_em),
			static_cast<float>(padded_w),
			static_cast<float>(padded_h),
		};

		glyphs[cp] = glyph(
			glyph::info{
				.ft_glyph_index = ft_index,
				.atlas_uv = atlas_uv,
				.plane_bounds = plane_bounds,
				.x_advance = x_advance_em,
			}
		);
		++baked_count;
	}

	log::println(
		log::level::info,
		log::category::assets,
		"font bake [{}]: baked={}, skipped(non-space)={}, atlas={}x{} ({} bytes)",
		src.filename().string(),
		baked_count,
		skipped_count,
		atlas_width,
		atlas_height,
		atlas_data.size()
	);

	const auto debug_atlas_path = config::baked_resource_path / "Fonts" / (src.stem().string() + "_atlas_debug.png");
	if (!image::write_png(debug_atlas_path, static_cast<std::uint32_t>(atlas_width), static_cast<std::uint32_t>(atlas_height), channels, atlas_data.data())) {
		log::println(
			log::level::warning,
			log::category::assets,
			"font bake [{}]: failed to write debug atlas PNG to {}",
			src.filename().string(),
			debug_atlas_path.string()
		);
	}

	destroyFont(font_handle);
	deinitializeFreetype(ft_handle);
	FT_Done_Face(ft_face);
	FT_Done_FreeType(ft_lib);

	out.source_path_relative = src.lexically_relative(config::resource_path).string();
	out.ascender = static_cast<float>(font_metrics.ascenderY);
	out.descender = static_cast<float>(font_metrics.descenderY);
	out.pixel_range = pixel_range;
	out.atlas_width = static_cast<std::uint32_t>(atlas_width);
	out.atlas_height = static_cast<std::uint32_t>(atlas_height);
	out.channels = channels;
	out.rgba.storage = std::move(atlas_data);
	out.glyphs = std::move(glyphs);
	return true;
}
