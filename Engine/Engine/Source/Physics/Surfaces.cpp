#include "Physics/Surfaces.h"

const std::unordered_map<Engine::Surfaces::SurfaceType, Engine::Surfaces::SurfaceProperties> surfaceMap = {
	{Engine::Surfaces::SurfaceType::Concrete,
		{0.8f, 0.2f, Engine::seconds(0.5f), 0.9f}},
	{Engine::Surfaces::SurfaceType::Grass,
		{0.6f, 0.1f, Engine::seconds(0.5f), 0.7f}},
	{Engine::Surfaces::SurfaceType::Water,
		{0.1f, 0.0f, Engine::seconds(0.5f), 0.1f}},
	{Engine::Surfaces::SurfaceType::Sand,
		{0.4f, 0.1f, Engine::seconds(0.5f), 0.6f}},
	{Engine::Surfaces::SurfaceType::Gravel,
		{0.5f, 0.1f, Engine::seconds(0.5f), 0.8f}},
	{Engine::Surfaces::SurfaceType::Asphalt,
		{0.7f, 0.2f, Engine::seconds(0.5f), 0.8f}}
};

Engine::Surfaces::SurfaceProperties Engine::Surfaces::getSurfaceProperties(const SurfaceType& surfaceType) {
	return surfaceMap.at(surfaceType);
}