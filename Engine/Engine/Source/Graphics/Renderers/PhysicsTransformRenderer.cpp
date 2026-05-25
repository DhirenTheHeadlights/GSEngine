module gse.graphics;

import std;

import :physics_transform_renderer;
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
import gse.physics;

namespace gse::renderer::physics_transform {
	struct [[= shaders::shader_struct]] physics_mapping {
		std::uint32_t body_index;
		std::uint32_t instance_index;
		vec3f center_of_mass;
	};

	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t mapping_count;
		std::uint32_t body_count;
	};

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::ssbo_readonly
	]] body_data {
		using element = vbd::body_state;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] mapping_data {
		using element = physics_mapping;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::ssbo_readwrite
	]] instance_data_buffer {
		using element = shaders::common::instance_data;
	};

	using shader_binding_types = type_pack<body_data, mapping_data, instance_data_buffer>;
	using shader_types = type_pack<physics_mapping>;

	using entry = gpu::compute_entry<
		gpu::body_path<"Compute/physics_instance_transform">,
		gpu::layout<"physics_instance_transform">,
		gpu::types<shaders::common::shader_types, vbd::shader_types, shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::helpers<"VBDPhysics/vbd_shared">,
		gpu::threads<64>,
		gpu::push_constant<push_constants>,
		gpu::system_values<gpu::dispatch_thread_id>
	>;
}

auto gse::renderer::physics_transform::system::run(run_context& ctx, const gpu::context::data& gpu_s, const asset::data& assets_s, data& d) -> async::task<> {
	d.pipeline =
		gpu::build_compute_program(
			*gpu_s.device,
			*gpu_s.shader_registry,
			*gpu_s.bindless_textures,
			entry::pod,
			gpu_s.bindless_heaps.get()
		);

	d.initialized = true;

	co_return;
}

auto gse::renderer::physics_transform::system::frame(frame_context& ctx, shared_view<gpu::context> gpu_s, data& d, shared_view<geometry_collector::system> gc_r) -> async::task<> {
	const auto& solver_infos = ctx.read_channel<physics::gpu_solver_frame_info>();

	if (solver_infos.empty()) {
		co_return;
	}

	if (!solver_infos[0].snapshot || solver_infos[0].body_count == 0) {
		co_return;
	}

	const auto& info = solver_infos[0];
	const auto& snapshot = *info.snapshot;

	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto& render_items = ctx.read_channel<geometry_collector::render_data>();

	if (!render_items.empty() && render_items[0].physics_mapping_count > 0) {
		const auto& data = render_items[0];
		const auto required = data.physics_mapping_count * sizeof(geometry_collector::physics_mapping_entry);

		d.cached_mapping_count = data.physics_mapping_count;

		if (d.mapping_buffer_size < required) {
			for (std::size_t i = 0; i < per_frame_resource<vulkan::bindless_buffer>::frames_in_flight; ++i) {
				d.mapping_buffers[i] = vulkan::bindless_buffer::create(
					gpu_s.device->allocator(),
					*gpu_s.bindless_heaps,
					{
						.size = required,
						.usage = gpu::buffer_flag::storage,
						.data = data.physics_mappings.data()
					}
				);
			}
			d.mapping_buffer_size = required;
		}
		else {
			d.mapping_buffers[frame_index].buffer().host_write(data.physics_mappings.data(), required);
		}
	}

	if (d.cached_mapping_count == 0) {
		co_return;
	}

	d.body_views[frame_index].rebind_storage(
		gpu_s.device->allocator(),
		*gpu_s.bindless_heaps,
		snapshot,
		0,
		info.body_count * info.body_stride
	);
	d.instance_views[frame_index].rebind_storage(
		gpu_s.device->allocator(),
		*gpu_s.bindless_heaps,
		gc_r.instance_buffer[frame_index],
		0,
		gc_r.instance_buffer[frame_index].size()
	);

	const std::uint32_t workgroups = (d.cached_mapping_count + 63) / 64;

	auto rec = co_await gpu::pass<system>(ctx)
		.pipeline(d.pipeline)
		.after<geometry_collector::system, vbd::vbd_state_copy_stage>();

	rec.dispatch<entry>(
		{
			.mapping_count = d.cached_mapping_count,
			.body_count = info.body_count,
		},
		{
			.body_data = d.body_views[frame_index].slot(),
			.mapping_data = d.mapping_buffers[frame_index].slot(),
			.instance_data_buffer = d.instance_views[frame_index].slot(),
		},
		vec3u{ workgroups, 1u, 1u }
	);
}
