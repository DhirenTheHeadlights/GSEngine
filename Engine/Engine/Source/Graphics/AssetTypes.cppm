export module gse.graphics:asset_types;

import std;

import gse.assets;
import gse.containers;
import gse.gpu;

import :texture;
import :font;
import :model;
import :clip;
import :skinned_model;

export namespace gse::graphics {
	using simulation_asset_types = type_pack<model, clip_asset, skinned_model>;
	using presentation_asset_types = type_pack<texture, font>;
	using asset_types = assets::append<simulation_asset_types, presentation_asset_types>;
}

namespace gse::graphics {
	static_assert(asset::has_compile_path<clip_asset>, "clip_asset declares a source format but is not bakeable");
	static_assert(asset::has_compile_path<skinned_model>, "skinned_model declares a source format but is not bakeable");

	static_assert(format_of<clip_asset::baked>().source_dir == "Clips");
	static_assert(format_of<clip_asset::baked>().baked_dir == "Clips");
	static_assert(format_of<clip_asset::baked>().magic == 0x47434C50);

	static_assert(format_of<skinned_model::baked>().source_dir == "SkinnedModels");
	static_assert(format_of<skinned_model::baked>().baked_dir == "SkinnedModels");
	static_assert(format_of<skinned_model::baked>().baked_ext == ".gsmdl");
	static_assert(format_of<skinned_model::baked>().magic == 0x47534D44);
}
