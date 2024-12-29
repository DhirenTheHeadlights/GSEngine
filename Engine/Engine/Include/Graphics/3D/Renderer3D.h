#pragma once

#include "Graphics/RenderComponent.h"
#include "Graphics/3D/Camera.h"
#include "Lights/LightSourceComponent.h"

namespace gse::renderer3d {
	void initialize();
	void initialize_objects();
	void render();

	camera& get_camera();
}
