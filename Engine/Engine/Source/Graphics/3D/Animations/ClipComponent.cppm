export module gse.graphics:clip_component;

import std;

import gse.utility;
import :clip;
import :resource_loader;

export namespace gse {
    struct clip_component_data {
        resource::handle<clip_asset> clip;
        time scale = 1.f;
        bool loop = true;
    };

    struct clip_component : component<clip_component_data> {
        using component::component;

        time t = 0.f;
        bool playing = true;
    };
}
