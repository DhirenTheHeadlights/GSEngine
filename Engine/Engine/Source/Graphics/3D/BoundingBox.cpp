#include "Graphics/3D/BoundingBox.h"

#include <iostream>

#include "Core/EngineCore.h"

gse::vec3<gse::length> gse::get_left_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length halfWidth = meters(bounding_box.get_size().as<Meters>().x / 2.0f);
	return center - vec3<Meters>(halfWidth, 0.0f, 0.0f);
}

gse::vec3<gse::length> gse::get_right_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length halfWidth = meters(bounding_box.get_size().as<Meters>().x / 2.0f);
	return center + vec3<Meters>(halfWidth, 0.0f, 0.0f);
}

gse::vec3<gse::length> gse::get_front_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length halfDepth = meters(bounding_box.get_size().as<Meters>().z / 2.0f);
	return center - vec3<Meters>(0.0f, 0.0f, halfDepth);
}

gse::vec3<gse::length> gse::get_back_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length halfDepth = meters(bounding_box.get_size().as<Meters>().z / 2.0f);
	return center + vec3<Meters>(0.0f, 0.0f, halfDepth);
}
