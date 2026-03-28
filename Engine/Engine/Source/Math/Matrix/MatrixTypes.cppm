export module gse.math:matrix_types;

import std;

import :mixed_mat;
import :units;

export namespace gse {
	using spatial_spec = unit_spec<displacement, displacement, displacement, float>;
	using clip_spec = unit_spec<float, float, float, float>;

	using spatial_matrix = mixed_mat<spatial_spec, spatial_spec>;
	using view_matrix = spatial_matrix;
	using projection_matrix = mixed_mat<spatial_spec, clip_spec>;
	using view_projection_matrix = projection_matrix;
}
