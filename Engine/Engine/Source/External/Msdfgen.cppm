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

	using ::msdfgen::initializeFreetype;
	using ::msdfgen::deinitializeFreetype;
	using ::msdfgen::loadFont;
	using ::msdfgen::loadGlyph;
	using ::msdfgen::destroyFont;
	using ::msdfgen::edgeColoringSimple;
	using ::msdfgen::generateMSDF;
}
