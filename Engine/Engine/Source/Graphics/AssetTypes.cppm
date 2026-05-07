export module gse.graphics:asset_types;

import gse.containers;
import gse.gpu;

import :texture;
import :font;
import :model;
import :skinned_model;
import :skeleton;
import :clip;

export namespace gse::graphics {
    using asset_types = type_pack<
        texture,
        font,
        model,
        shader,
        shader_layout,
        skinned_model,
        skeleton,
        clip_asset
    >;
}
