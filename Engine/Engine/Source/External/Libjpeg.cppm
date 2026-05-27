module;

#include <cstdio>
#include <jpeglib.h>
#include <csetjmp>

export module gse.libjpeg;

export {
	using ::jpeg_common_struct;
	using ::jpeg_decompress_struct;
	using ::jpeg_compress_struct;
	using ::jpeg_error_mgr;
	using ::j_common_ptr;
	using ::j_decompress_ptr;
	using ::j_compress_ptr;
	using ::JSAMPLE;
	using ::JSAMPROW;
	using ::JDIMENSION;
	using ::J_COLOR_SPACE;
	using ::JOCTET;

	using ::jpeg_destroy_decompress;
	using ::jpeg_destroy_compress;
	using ::jpeg_stdio_src;
	using ::jpeg_stdio_dest;
	using ::jpeg_read_header;
	using ::jpeg_start_decompress;
	using ::jpeg_finish_decompress;
	using ::jpeg_start_compress;
	using ::jpeg_finish_compress;
	using ::jpeg_set_defaults;
	using ::jpeg_set_quality;
	using ::jpeg_read_scanlines;
	using ::jpeg_write_scanlines;
	using ::jpeg_std_error;

	inline auto jpeg_create_decompress_wrap(j_decompress_ptr cinfo) -> void {
		jpeg_create_decompress(cinfo);
	}

	inline auto jpeg_create_compress_wrap(j_compress_ptr cinfo) -> void {
		jpeg_create_compress(cinfo);
	}

	inline constexpr int jpeg_true = TRUE;
	inline constexpr int jpeg_false = FALSE;
	inline constexpr ::J_COLOR_SPACE jpeg_color_space_rgb = JCS_RGB;
	inline constexpr ::J_COLOR_SPACE jpeg_color_space_gray = JCS_GRAYSCALE;

	template <typename Body, typename OnError>
	auto jpeg_guarded(std::jmp_buf& buf, Body&& body, OnError&& on_error) -> void {
		if (setjmp(buf)) {
			on_error();
			return;
		}
		body();
	}
}
