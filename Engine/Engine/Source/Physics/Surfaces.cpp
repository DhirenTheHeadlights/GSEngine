#include "Physics/Surfaces.h"

const std::unordered_map<gse::Surfaces::SurfaceType, gse::Surfaces::SurfaceProperties> surfaceMap = {
	{gse::Surfaces::SurfaceType::Concrete,
		{0.8f, 0.2f, gse::seconds(0.5f), 0.9f}},
	{gse::Surfaces::SurfaceType::Grass,
		{0.6f, 0.1f, gse::seconds(0.5f), 0.7f}},
	{gse::Surfaces::SurfaceType::Water,
		{0.1f, 0.0f, gse::seconds(0.5f), 0.1f}},
	{gse::Surfaces::SurfaceType::Sand,
		{0.4f, 0.1f, gse::seconds(0.5f), 0.6f}},
	{gse::Surfaces::SurfaceType::Gravel,
		{0.5f, 0.1f, gse::seconds(0.5f), 0.8f}},
	{gse::Surfaces::SurfaceType::Asphalt,
		{0.7f, 0.2f, gse::seconds(0.5f), 0.8f}}
};

gse::Surfaces::SurfaceProperties gse::Surfaces::getSurfaceProperties(const SurfaceType& surfaceType) {
	return surfaceMap.at(surfaceType);
}