export module gse.graphics.renderer3d;

import gse.graphics.camera;

export namespace gse::renderer3d {
	auto initialize() -> void;
	auto initialize_objects() -> void;
	auto render() -> void;

	auto get_camera() -> camera&;
}
