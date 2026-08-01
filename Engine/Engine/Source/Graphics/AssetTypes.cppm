export module gse.graphics:asset_types;

import gse.assets;
import gse.containers;
import gse.gpu;

import :texture;
import :font;
import :model;
import :clip;
import :skinned_model;

export namespace gse::graphics {
	using asset_types = type_pack<texture, font, model, clip_asset, skinned_model>;
}

namespace gse::graphics {
	static_assert(asset::has_compile_path<clip_asset>, "clip_asset declares a source format but is not bakeable");
	static_assert(asset::has_compile_path<skinned_model>, "skinned_model declares a source format but is not bakeable");
}
