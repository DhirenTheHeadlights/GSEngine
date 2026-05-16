module gse.graphics;

import std;

import :skin_compute_renderer;
import :geometry_collector;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

namespace gse::renderer::skin_compute {
	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t joint_count;
		std::uint32_t instance_count;
		std::uint32_t local_pose_stride;
		std::uint32_t skin_stride;
	};

	struct [[= shaders::binding<0, 0>{}, = shaders::ssbo_readonly]] skeleton_data {
		using element = geometry_collector::joint_data;
	};

	struct [[= shaders::binding<0, 1>{}, = shaders::ssbo_readonly]] local_poses {
		using element = mat4f;
	};

	struct [[= shaders::binding<0, 2>{}, = shaders::ssbo_readwrite]] skin_matrices {
		using element = mat4f;
	};

	using shader_binding_types = type_pack<skeleton_data, local_poses, skin_matrices>;
	using shader_types = type_pack<geometry_collector::joint_data>;

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

auto gse::renderer::skin_compute::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, const geometry_collector::system::data& gc, data& d) -> async::task<> {
	assert(static_cast<bool>(gc.skeleton_buffer), "skin_compute::initialize: gc.skeleton_buffer is null - geometry_collector::initialize did not run before skin_compute::initialize");

	d.pipeline = gpu::build_compute_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	constexpr std::size_t skin_buffer_size = geometry_collector::system::data::max_skin_matrices * sizeof(mat4f);
	constexpr std::size_t local_pose_size = geometry_collector::system::data::max_skin_matrices * sizeof(mat4f);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.descriptors[i] = gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);

		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.descriptors[i])
			.buffer<skeleton_data>(gc.skeleton_buffer, 0, geometry_collector::system::data::max_joints * sizeof(geometry_collector::joint_data))
			.buffer<local_poses>(gc.local_pose_buffer[i], 0, local_pose_size)
			.buffer<skin_matrices>(gc.skin_buffer[i], 0, skin_buffer_size)
			.commit();
	}

	co_return;
}

auto gse::renderer::skin_compute::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, const data& d, shared_view<geometry_collector::system> gc_r) -> async::task<> {
	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();
	if (render_items.empty()) {
		co_return;
	}

	const auto& data = render_items.front();
	if (data.pending_compute_instance_count == 0) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	const gpu::typed_push_constants<push_constants> skin_pc{
		.data = {
			.joint_count = gc_r.current_joint_count,
			.instance_count = data.pending_compute_instance_count,
			.local_pose_stride = gc_r.current_joint_count,
			.skin_stride = gc_r.current_joint_count,
		},
		.stages = gpu::stage_flag::compute,
	};

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline);

	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.push(d.pipeline, skin_pc);
	rec.dispatch(data.pending_compute_instance_count, 1, 1);
}
