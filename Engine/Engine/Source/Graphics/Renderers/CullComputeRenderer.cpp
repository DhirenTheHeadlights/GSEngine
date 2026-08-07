module gse.graphics:cull_compute_renderer_impl;

import std;

import :cull_compute_renderer;
import :geometry_collector;
import :camera_system;


import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.math;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;
import gse.ecs;

namespace gse::renderer::cull_compute {
	struct [[= shaders::shader_struct]] frustum_data {
		vec4f planes[6];
	};

	struct [[= shaders::shader_struct]] batch_info {
		std::uint32_t first_instance;
		std::uint32_t instance_count;
		vec3<length> aabb_min;
		vec3<length> aabb_max;
	};

	struct [[= shaders::shader_struct]] push_constants {
		std::uint32_t batch_offset;
		std::uint32_t indirect_stride;
	};

	struct [[= shaders::binding<0, 0>{}]] frustum_ubo {
		using element = frustum_data;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] batches {
		using element = batch_info;
	};

	struct [[
		= shaders::binding<0, 2>{},
		= shaders::rw_byte_address_buffer
	]] indirect_commands {};

	using shader_binding_types = type_pack<frustum_ubo, batches, indirect_commands>;
	using shader_types = type_pack<frustum_data, batch_info>;

	using entry = gpu::compute_entry<gpu::body_path<"Compute/cull_instances">, gpu::types<shader_types>, gpu::bindings<shader_binding_types>, gpu::threads<1>, gpu::push_constant<push_constants>, gpu::system_values<gpu::group_id>>;
}

auto gse::renderer::cull_compute::init(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, const shared_view<geometry_collector::data> gc_r, data& d) -> async::task<> {
	d.pipeline = gpu::build_compute_program(*gpu_s.device, entry::pod);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		constexpr std::size_t frustum_size = sizeof(std::array<vec4f, 6>);
		d.frustum_buffer[i] = gpu_s.device->create_buffer(
			{
				.size = frustum_size,
				.stride = sizeof(std::array<vec4f, 6>),
				.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst },
				.bindless = true
			}
		);

		constexpr std::size_t batch_info_size = geometry_collector::render_data::max_batches * 2 * sizeof(batch_info);
		d.batch_info_buffer[i] = gpu_s.device->create_buffer(
			{
				.size = batch_info_size,
				.stride = sizeof(batch_info),
				.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst },
				.bindless = true
			}
		);
	}

	return {};
}

auto gse::renderer::cull_compute::frame(context& ctx, shared_view<gpu::context::data> gpu_s, shared_view<geometry_collector::data> gc_r, const data& d) -> async::task<> {
	if (!d.enabled) {
		co_return;
	}

	const auto& items = ctx.read_channel<geometry_collector::render_data>();
	if (items.empty()) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto& data = items[0];
	const auto frame_index = gpu_s.render_graph->current_frame();

	const auto normal_count = static_cast<std::uint32_t>(data.normal_batches.size());

	if (normal_count == 0) {
		co_return;
	}

	const view_projection_matrix view_proj = data.proj * data.view;
	const auto planes = extract_frustum_planes(view_proj);
	d.frustum_buffer[frame_index].host_write(planes);

	using batch_info = renderer::cull_compute::batch_info;
	std::vector<batch_info> batch_staging(normal_count);

	for (std::size_t i = 0; i < data.normal_batches.size(); ++i) {
		const auto& b = data.normal_batches[i];
		batch_staging[i] = {
			.first_instance = b.first_instance,
			.instance_count = b.instance_count,
			.aabb_min = b.world_aabb_min,
			.aabb_max = b.world_aabb_max,
		};
	}

	if (!batch_staging.empty()) {
		d.batch_info_buffer[frame_index].host_write(
			batch_staging.data(),
			batch_staging.size() * sizeof(batch_info)
		);
	}

	auto rec = co_await gpu::pass<^^gse::renderer::cull_compute::frame>(ctx).pipeline(d.pipeline);

	rec.dispatch<entry>(
		{
			.batch_offset = 0,
			.indirect_stride = static_cast<std::uint32_t>(sizeof(gpu::draw_mesh_tasks_indirect_command)),
		},
		{
			.frustum_ubo = d.frustum_buffer[frame_index].slot(),
			.batches = d.batch_info_buffer[frame_index].slot(),
			.indirect_commands = gc_r.normal_indirect_commands_buffer[frame_index].slot(),
		},
		vec3u{ normal_count, 1u, 1u }
	);
}
