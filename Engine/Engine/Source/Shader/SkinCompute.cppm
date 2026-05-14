export module gse.shader:skin_compute;

import std;

import gse.containers;
import gse.gpu;
import gse.math;

export namespace gse::shaders::skin_compute {
	struct [[= shader_struct]] joint_data {
		mat4f inverse_bind;
		std::uint32_t parent_index;
		std::uint32_t pad0;
		std::uint32_t pad1;
		std::uint32_t pad2;
	};

	struct [[= shader_struct]] push_constants {
		std::uint32_t joint_count;
		std::uint32_t instance_count;
		std::uint32_t local_pose_stride;
		std::uint32_t skin_stride;
	};

	struct [[= binding<0, 0>{}, = ssbo_readonly]] skeleton_data {
		using element = joint_data;
	};

	struct [[= binding<0, 1>{}, = ssbo_readonly]] local_poses {
		using element = mat4f;
	};

	struct [[= binding<0, 2>{}, = ssbo_readwrite]] skin_matrices {
		using element = mat4f;
	};

	using shader_binding_types = type_pack<
		skeleton_data,
		local_poses,
		skin_matrices
	>;

	using shader_types = type_pack<joint_data>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/skin_compute">,
		gpu::layout<"skin_compute">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::threads<64>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::group_id, gpu::group_thread_id>
	>;
}

static_assert(sizeof(gse::shaders::skin_compute::joint_data) == gse::shaders::slang_scalar_size<gse::shaders::skin_compute::joint_data>());
static_assert(sizeof(gse::shaders::skin_compute::push_constants) == gse::shaders::slang_scalar_size<gse::shaders::skin_compute::push_constants>());
