module gse.physics.bounding_box;

import gse.physics.math.vector;
import gse.physics.math.units;

gse::axis_aligned_bounding_box::axis_aligned_bounding_box(const vec3<length>& center, const vec3<length>& size) 
	: upper_bound(center + size / 2.0f), lower_bound(center - size / 2.0f) {}


gse::axis_aligned_bounding_box::axis_aligned_bounding_box(const vec3<length>& center, const length& width, const length& height, const length& depth)
	: axis_aligned_bounding_box(center, vec3<length>(width, height, depth)) {}

auto gse::axis_aligned_bounding_box::set_position(const vec3<length>& center) -> void {
	const vec3<length> half_size = (upper_bound - lower_bound) / 2.0f;
	upper_bound = center + half_size;
	lower_bound = center - half_size;
}

auto gse::axis_aligned_bounding_box::get_center() const -> vec3<length> {
	return (upper_bound + lower_bound) / 2.0f;
}

auto gse::axis_aligned_bounding_box::get_size() const -> vec3<length> {
	return upper_bound - lower_bound;
}