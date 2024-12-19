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

// For side, 0 is bottom, 1 is top, 2 is left, 3 is right, 4 is front, 5 is back
void gse::set_bounding_box_position_by_axis(bounding_box& bounding_box, const vec3<length>& new_position, const int side) {
	const glm::vec3 half_size = (bounding_box.get_size() / 2.0f).as_default_units();
	const glm::vec3 position = new_position.as_default_units();

	switch (side) {
	case 0:
		bounding_box.upper_bound = vec3<length>(position.x, position.y + half_size.y, position.z);
		bounding_box.lower_bound = vec3<length>(position.x, position.y - half_size.y, position.z);
		break;
	case 1:
		bounding_box.upper_bound = vec3<length>(position.x, position.y + half_size.y, position.z);
		bounding_box.lower_bound = vec3<length>(position.x, position.y - half_size.y, position.z);
		break;
	case 2:
		bounding_box.upper_bound = vec3<length>(position.x + half_size.x, position.y, position.z);
		bounding_box.lower_bound = vec3<length>(position.x - half_size.x, position.y, position.z);
		break;
	case 3:
		bounding_box.upper_bound = vec3<length>(position.x + half_size.x, position.y, position.z);
		bounding_box.lower_bound = vec3<length>(position.x - half_size.x, position.y, position.z);
		break;
	case 4:
		bounding_box.upper_bound = vec3<length>(position.x, position.y, position.z + half_size.z);
		bounding_box.lower_bound = vec3<length>(position.x, position.y, position.z - half_size.z);
		break;
	case 5:
		bounding_box.upper_bound = vec3<length>(position.x, position.y, position.z + half_size.z);
		bounding_box.lower_bound = vec3<length>(position.x, position.y, position.z - half_size.z);
		break;
	default:
		std::cerr << "Invalid side for set_bounding_box_position_by_axis" << std::endl;
		break;
	}
}
