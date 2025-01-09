#pragma once

#include "Graphics/3D/Camera.h"

namespace gse::renderer3d {
	auto initialize() -> void;
	auto initialize_objects() -> void;
	auto render() -> void;

	auto get_camera() -> camera&;
}
