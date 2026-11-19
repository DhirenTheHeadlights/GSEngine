module;

#include <freetype/freetype.h>

export module gse.freetype;

export {
	using ::FT_Library;
	using ::FT_Face;
	using ::FT_GlyphSlot;
	using ::FT_Vector;
	using ::FT_Error;
	using ::FT_Long;
	using ::FT_UInt;
	using ::FT_Int32;
	using ::FT_Kerning_Mode;
	using ::FT_Kerning_Mode_;

	using ::FT_Init_FreeType;
	using ::FT_Done_FreeType;
	using ::FT_New_Face;
	using ::FT_Done_Face;
	using ::FT_Set_Pixel_Sizes;
	using ::FT_Load_Char;
	using ::FT_Load_Glyph;
	using ::FT_Get_Char_Index;
	using ::FT_Get_Kerning;
}

export namespace gse {
	inline constexpr ::FT_Int32 freetype_load_no_scale = FT_LOAD_NO_SCALE;
	inline constexpr ::FT_UInt freetype_kerning_unscaled = FT_KERNING_UNSCALED;
}
