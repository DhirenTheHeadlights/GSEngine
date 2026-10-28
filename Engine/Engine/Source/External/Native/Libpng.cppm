module;

#include <cstdint>
#include <png.h>
#include <csetjmp>

export module gse.libpng;

export {
	using ::png_struct;
	using ::png_info;
	using ::png_structp;
	using ::png_infop;
	using ::png_byte;
	using ::png_bytep;
	using ::png_uint_32;
	using ::png_size_t;

	using ::png_create_read_struct;
	using ::png_create_write_struct;
	using ::png_create_info_struct;
	using ::png_destroy_read_struct;
	using ::png_destroy_write_struct;
	using ::png_init_io;
	using ::png_read_info;
	using ::png_read_image;
	using ::png_read_update_info;
	using ::png_write_info;
	using ::png_write_image;
	using ::png_write_end;
	using ::png_get_image_width;
	using ::png_get_image_height;
	using ::png_get_color_type;
	using ::png_get_bit_depth;
	using ::png_get_channels;
	using ::png_get_rowbytes;
	using ::png_get_valid;
	using ::png_set_strip_16;
	using ::png_set_palette_to_rgb;
	using ::png_set_expand_gray_1_2_4_to_8;
	using ::png_set_tRNS_to_alpha;
	using ::png_set_filler;
	using ::png_set_gray_to_rgb;
	using ::png_set_IHDR;
	using ::png_sig_cmp;
	using ::png_set_longjmp_fn;

	inline constexpr const char* png_libpng_ver_string = PNG_LIBPNG_VER_STRING;

	inline constexpr int png_color_type_palette = PNG_COLOR_TYPE_PALETTE;
	inline constexpr int png_color_type_gray = PNG_COLOR_TYPE_GRAY;
	inline constexpr int png_color_type_gray_alpha = PNG_COLOR_TYPE_GRAY_ALPHA;
	inline constexpr int png_color_type_rgb = PNG_COLOR_TYPE_RGB;
	inline constexpr int png_color_type_rgba = PNG_COLOR_TYPE_RGBA;
	inline constexpr int png_filler_after = PNG_FILLER_AFTER;
	inline constexpr int png_interlace_none = PNG_INTERLACE_NONE;
	inline constexpr int png_compression_type_default = PNG_COMPRESSION_TYPE_DEFAULT;
	inline constexpr int png_filter_type_default = PNG_FILTER_TYPE_DEFAULT;
	inline constexpr std::uint32_t png_info_trns = PNG_INFO_tRNS;

	inline auto png_jmpbuf_of(png_structp png) -> jmp_buf& {
		return *reinterpret_cast<jmp_buf*>(png_set_longjmp_fn(png, std::longjmp, sizeof(jmp_buf)));
	}

	template <typename Body, typename OnError>
	auto png_guarded(png_structp png, Body&& body, OnError&& on_error) -> void {
		if (setjmp(png_jmpbuf_of(png))) {
			on_error();
			return;
		}
		body();
	}
}
