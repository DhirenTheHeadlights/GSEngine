module;

#include <msdfgen.h>
#include "ext/import-font.h"

export module gse.msdfgen;

export namespace msdfgen {
	using ::msdfgen::FreetypeHandle;
	using ::msdfgen::FontHandle;
	using ::msdfgen::Shape;
	using ::msdfgen::Bitmap;
	using ::msdfgen::Range;
	using ::msdfgen::Vector2;
	using ::msdfgen::Projection;
	using ::msdfgen::FontMetrics;
	using ::msdfgen::FontCoordinateScaling;
	using ::msdfgen::ErrorCorrectionConfig;
	using ::msdfgen::unicode_t;

	using ::msdfgen::initializeFreetype;
	using ::msdfgen::deinitializeFreetype;
	using ::msdfgen::loadFont;
	using ::msdfgen::loadGlyph;
	using ::msdfgen::destroyFont;
	using ::msdfgen::getFontMetrics;
	using ::msdfgen::edgeColoringByDistance;
	using ::msdfgen::edgeColoringInkTrap;
	using ::msdfgen::generateMSDF;
	using ::msdfgen::generateMTSDF;
}

export namespace msdfgen_consts {
	inline constexpr ::msdfgen::FontCoordinateScaling em_normalized = ::msdfgen::FONT_SCALING_EM_NORMALIZED;
	inline constexpr ::msdfgen::ErrorCorrectionConfig::Mode edge_priority =
		::msdfgen::ErrorCorrectionConfig::EDGE_PRIORITY;
	inline constexpr ::msdfgen::ErrorCorrectionConfig::DistanceCheckMode always_check_distance =
		::msdfgen::ErrorCorrectionConfig::ALWAYS_CHECK_DISTANCE;
}
