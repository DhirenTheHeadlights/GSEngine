export module gse.core.multiplayer_world;

import std;
import gse.core.id;

export namespace gse::multiplayer_world {
	struct instance {
		virtual ~instance() = default;
		virtual auto initialize() -> void = 0;
		virtual auto update() -> void = 0;
		virtual auto render() -> void = 0;
	};
}