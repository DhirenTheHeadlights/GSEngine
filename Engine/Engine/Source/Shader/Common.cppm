export module gse.shader:common;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::common {
	struct [[= shader_struct]] camera_data {
		mat4f view;
		mat4f proj;
		mat4f inv_view;
	};

	struct [[= shader_struct]] instance_data {
		mat4f model_matrix;
		mat4f normal_matrix;
		std::uint32_t skin_offset;
		std::uint32_t joint_count;
		std::uint32_t material_index;
		std::uint32_t pad0;
	};

	using shader_types = type_pack<camera_data, instance_data>;
}

static_assert(sizeof(gse::shaders::common::camera_data) == gse::shaders::slang_scalar_size<gse::shaders::common::camera_data>());
static_assert(sizeof(gse::shaders::common::instance_data) == gse::shaders::slang_scalar_size<gse::shaders::common::instance_data>());
