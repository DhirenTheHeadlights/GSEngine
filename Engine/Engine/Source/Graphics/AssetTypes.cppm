export module gse.graphics:asset_types;

import gse.containers;
import gse.gpu;

import :texture;
import :font;
import :model;

export namespace gse::graphics {
	using asset_types = type_pack<texture, font, model>;
}
