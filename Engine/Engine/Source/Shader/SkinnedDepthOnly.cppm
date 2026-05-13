export module gse.shader:skinned_depth_only;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

import :common;

export namespace gse::shaders::skinned_depth_only {
	struct [[= binding<0, 0>{}]] camera_ubo {
		using element = common::camera_data;
	};

	struct [[= binding<1, 0>{}, = ssbo_readonly]] skin_matrices {
		using element = mat4f;
	};

	struct [[= binding<1, 1>{}, = ssbo_readonly]] instance_data_buffer {
		using element = common::instance_data;
	};

	using shader_binding_types = type_pack<
		camera_ubo,
		skin_matrices,
		instance_data_buffer
	>;
}
