#include "Engine/Graphics/BoundingBox.h"

#include <iostream>

#include "Engine/Core/EngineCore.h"

Engine::Vec3<Engine::Length> Engine::getLeftBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Units::Meters halfWidth = boundingBox.getSize().as<Units::Meters>().x / 2.0f;
	return center - Vec3<Units::Meters>(halfWidth, 0.0f, 0.0f);
}

Engine::Vec3<Engine::Length> Engine::getRightBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Units::Meters halfWidth = boundingBox.getSize().as<Units::Meters>().x / 2.0f;
	return center + Vec3<Units::Meters>(halfWidth, 0.0f, 0.0f);
}

Engine::Vec3<Engine::Length> Engine::getFrontBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Units::Meters halfDepth = boundingBox.getSize().as<Units::Meters>().z / 2.0f;
	return center - Vec3<Units::Meters>(0.0f, 0.0f, halfDepth);
}

Engine::Vec3<Engine::Length> Engine::getBackBound(const BoundingBox& boundingBox) {
	const Vec3<Length> center = boundingBox.getCenter();
	const Units::Meters halfDepth = boundingBox.getSize().as<Units::Meters>().z / 2.0f;
	return center + Vec3<Units::Meters>(0.0f, 0.0f, halfDepth);
}
