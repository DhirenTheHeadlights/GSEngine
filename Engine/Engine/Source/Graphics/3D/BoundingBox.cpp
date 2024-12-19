#include "Graphics/3D/BoundingBox.h"

#include <iostream>

#include "Core/EngineCore.h"

gse::vec3<gse::length> gse::get_left_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length half_width = meters(bounding_box.get_size().as<units::meters>().x / 2.0f);
	return center - vec3<units::meters>(half_width, 0.0f, 0.0f);
}

gse::vec3<gse::length> gse::get_right_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length half_width = meters(bounding_box.get_size().as<units::meters>().x / 2.0f);
	return center + vec3<units::meters>(half_width, 0.0f, 0.0f);
}

gse::vec3<gse::length> gse::get_front_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length half_depth = meters(bounding_box.get_size().as<units::meters>().z / 2.0f);
	return center - vec3<units::meters>(0.0f, 0.0f, half_depth);
}

gse::vec3<gse::length> gse::get_back_bound(const bounding_box& bounding_box) {
	const vec3<length> center = bounding_box.get_center();
	const length half_depth = meters(bounding_box.get_size().as<units::meters>().z / 2.0f);
	return center + vec3<units::meters>(0.0f, 0.0f, half_depth);
}

void gse::set_position_lower_bound(physics::motion_component* component, bounding_box& bounding_box, const vec3<length>& new_position) {
	const vec3<length> half_size = (bounding_box.upper_bound - bounding_box.lower_bound) / 2.0f;
	bounding_box.upper_bound = new_position + half_size;
	bounding_box.lower_bound = new_position - half_size;

	component->current_position = bounding_box.get_center();
}


