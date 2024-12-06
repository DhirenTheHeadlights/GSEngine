#include "Graphics/3D/BoundingBox.h"

#include <iostream>

#include "Core/EngineCore.h"

Engine::Vec3<Engine::Length> Engine::getLeftBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfWidth = meters(boundingBox.getSize().as<Meters>().x / 2.0f);
	return center - Vec3<Meters>(halfWidth, 0.0f, 0.0f);
}

Engine::Vec3<Engine::Length> Engine::getRightBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfWidth = meters(boundingBox.getSize().as<Meters>().x / 2.0f);
	return center + Vec3<Meters>(halfWidth, 0.0f, 0.0f);
}

Engine::Vec3<Engine::Length> Engine::getFrontBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfDepth = meters(boundingBox.getSize().as<Meters>().z / 2.0f);
	return center - Vec3<Meters>(0.0f, 0.0f, halfDepth);
}

Engine::Vec3<Engine::Length> Engine::getBackBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Length halfDepth = meters(boundingBox.getSize().as<Meters>().z / 2.0f);
	return center + Vec3<Meters>(0.0f, 0.0f, halfDepth);
}
