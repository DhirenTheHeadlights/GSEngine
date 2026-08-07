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
	const std::string source_path = src.native_encoded_string();
	if (FT_New_Face(ft_lib, source_path.c_str(), 0, &ft_face)) {
		log::println(log::level::error, log::category::assets, "Failed to load font face from '{}'", src.generic_display_string());
		FT_Done_FreeType(ft_lib);
		return false;
	}

	msdfgen::FreetypeHandle* ft_handle = msdfgen::initializeFreetype();
	msdfgen::FontHandle* font_handle = loadFont(ft_handle, source_path.c_str());
	if (!font_handle) {
		log::println(log::level::error, log::category::assets, "Failed to load font into msdfgen: {}", src.generic_display_string());
		FT_Done_Face(ft_face);
		FT_Done_FreeType(ft_lib);
		msdfgen::deinitializeFreetype(ft_handle);
		return false;
	}

	msdfgen::FontMetrics font_metrics{};
	getFontMetrics(font_metrics, font_handle, msdfgen_consts::em_normalized);

	std::vector<char32_t> requested;
	requested.reserve(256);
	for (char32_t cp = 0x20; cp <= 0x7E; ++cp) {
		requested.push_back(cp);
	}
	for (char32_t cp = 0xA0; cp <= 0xFF; ++cp) {
		requested.push_back(cp);
	}
	for (const char32_t cp : { U'\u2013', U'\u2014', U'\u2018', U'\u2019', U'\u201C', U'\u201D', U'\u2026', U'\u2192' }) {
		requested.push_back(cp);
	}

	std::vector<char32_t> codepoints;
	codepoints.reserve(requested.size());
	std::ranges::copy_if(requested, std::back_inserter(codepoints), [ft_face](const char32_t cp) {
		return FT_Get_Char_Index(ft_face, static_cast<FT_UInt>(cp)) != 0;
	});

	const int glyph_count = static_cast<int>(codepoints.size());
	constexpr int cell = 192;
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
		}

		const auto bounds = shape.getBounds();
		const double shape_w = bounds.r - bounds.l;
		const double shape_h = bounds.t - bounds.b;
		const bool valid_bounds = has_geometry && shape_w > 0.0 && shape_h > 0.0;

		if (!valid_bounds) {
			if (cp != U' ' && cp != U'\u00A0') {
				log::println(
					log::level::warning,
					log::category::assets,
					"font bake [{}]: codepoint U+{:04X} has no usable geometry (loaded={}, contours={}, w={}, h={})",
					src.filename().generic_display_string(),
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
				src.filename().generic_display_string(),
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

		msdfgen::distanceSignCorrection(
			mtsdf,
			shape,
			msdfgen::Vector2{ scale, scale },
			msdfgen::Vector2{ translate_x, translate_y },
			msdfgen_consts::fill_nonzero
		);

		for (int y = 0; y < cell; ++y) {
			for (int x = 0; x < cell; ++x) {
				const int atlas_x = cell_x + x;
				const int atlas_y = cell_y + (cell - 1 - y);
				const std::size_t idx = (static_cast<std::size_t>(atlas_y) * atlas_width + atlas_x) * channels;
				const auto* px = mtsdf(x, y);
				for (std::uint32_t channel = 0; channel < channels; ++channel) {
					atlas_data[idx + channel] = static_cast<std::byte>(std::clamp(px[channel], 0.f, 1.f) * 255.f);
				}
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
		src.filename().generic_display_string(),
		baked_count,
		skipped_count,
		atlas_width,
		atlas_height,
		atlas_data.size()
	);

	std::unordered_map<std::uint64_t, float> kerning;
	for (const glyph& previous : std::views::values(glyphs)) {
		if (previous.ft_glyph_index() == 0) {
			continue;
		}
		for (const glyph& next : std::views::values(glyphs)) {
			if (next.ft_glyph_index() == 0) {
				continue;
			}
			FT_Vector value{};
			FT_Get_Kerning(ft_face, previous.ft_glyph_index(), next.ft_glyph_index(), freetype_kerning_unscaled, &value);
			const float normalized = static_cast<float>(value.x) / static_cast<float>(units_per_em);
			if (normalized != 0.0f) {
				const std::uint64_t key = (static_cast<std::uint64_t>(previous.ft_glyph_index()) << 32) | next.ft_glyph_index();
				kerning.emplace(key, normalized);
			}
		}
	}

	const std::filesystem::path& source_root = config::source_root_containing(src);
	const auto debug_atlas_path = config::baked_root_for_source(source_root) / "Fonts" / (src.stem().native_encoded_string() + "_atlas_debug.png");
	if (!image::write_png(debug_atlas_path, static_cast<std::uint32_t>(atlas_width), static_cast<std::uint32_t>(atlas_height), channels, atlas_data.data())) {
		log::println(
			log::level::warning,
			log::category::assets,
			"font bake [{}]: failed to write debug atlas PNG to {}",
			src.filename().generic_display_string(),
			debug_atlas_path.generic_display_string()
		);
	}

	destroyFont(font_handle);
	deinitializeFreetype(ft_handle);
	FT_Done_Face(ft_face);
	FT_Done_FreeType(ft_lib);

	out.ascender = static_cast<float>(font_metrics.ascenderY);
	out.descender = static_cast<float>(font_metrics.descenderY);
	out.pixel_range = pixel_range;
	out.atlas_width = static_cast<std::uint32_t>(atlas_width);
	out.atlas_height = static_cast<std::uint32_t>(atlas_height);
	out.channels = channels;
	out.rgba.storage = std::move(atlas_data);
	out.glyphs = std::move(glyphs);
	out.kerning = std::move(kerning);
	return true;
}
