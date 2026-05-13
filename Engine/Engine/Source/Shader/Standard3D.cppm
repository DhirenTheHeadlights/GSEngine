export module gse.shader:standard_3d;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

import :common;

export namespace gse::shaders::standard_3d {
	struct [[= binding<0, 0>{}]] camera_ubo {
		using element = common::camera_data;
	};

	struct [[= binding<1, 2>{}, = sampler2d]] diffuse_sampler {};

	struct [[= binding<1, 3>{}, = ssbo_readonly]] skin_matrices {
		using element = mat4f;
	};

	struct [[= binding<1, 4>{}, = ssbo_readonly]] instance_data_buffer {
		using element = common::instance_data;
	};

	using shader_binding_types = type_pack<camera_ubo, diffuse_sampler, skin_matrices, instance_data_buffer>;
}
