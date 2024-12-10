#include "Graphics/3D/BoundingBox.h"

#include <iostream>

#include "Core/EngineCore.h"

gse::Vec3<gse::Length> gse::getLeftBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfWidth = meters(boundingBox.getSize().as<Meters>().x / 2.0f);
	return center - Vec3<Meters>(halfWidth, 0.0f, 0.0f);
}

gse::Vec3<gse::Length> gse::getRightBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfWidth = meters(boundingBox.getSize().as<Meters>().x / 2.0f);
	return center + Vec3<Meters>(halfWidth, 0.0f, 0.0f);
}

gse::Vec3<gse::Length> gse::getFrontBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfDepth = meters(boundingBox.getSize().as<Meters>().z / 2.0f);
	return center - Vec3<Meters>(0.0f, 0.0f, halfDepth);
}

gse::Vec3<gse::Length> gse::getBackBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfDepth = meters(boundingBox.getSize().as<Meters>().z / 2.0f);
	return center + Vec3<Meters>(0.0f, 0.0f, halfDepth);
}
