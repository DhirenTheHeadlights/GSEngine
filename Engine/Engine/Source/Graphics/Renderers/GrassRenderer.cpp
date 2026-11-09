module gse.graphics:grass_renderer_impl;

import std;

import :camera_system;
import :geometry_collector;
import :grass_renderer;
import :lighting_source;
import :shared_shaders;

import gse.concurrency;
import gse.containers;
import gse.core;
import gse.ecs;
import gse.gpu;
import gse.gpu_record;
import gse.log;
import gse.math;
import gse.physics;

namespace gse::renderer::grass {
	auto build_tile_indices() -> std::vector<std::uint32_t>;

	auto create_field_render(
		gpu::device& device,
		std::uint32_t tile_count,
		field_render& fr
	) -> void;
}

auto gse::renderer::grass::build_tile_indices() -> std::vector<std::uint32_t> {
	constexpr auto& layout = physics::grass::layout;
	std::vector<std::uint32_t> indices;
	indices.reserve(index_count());
	for (std::uint32_t blade = 0; blade < layout.grass_tile_blade_count; ++blade) {
		const auto base = blade * layout.grass_side_vertices;
		const auto left = [base](const std::uint32_t s) {
			return base + 2 * s;
		};
		const auto right = [base](const std::uint32_t s) {
			return base + 2 * s + 1;
		};
		for (std::uint32_t s = 0; s + 1 < layout.grass_segments; ++s) {
			indices.insert(indices.end(), { left(s), right(s), right(s + 1), left(s), right(s + 1), left(s + 1) });
		}
		const auto last = layout.grass_segments - 1;
		indices.insert(indices.end(), { left(last), right(last), base + layout.grass_side_vertices - 1 });
	}
	return indices;
}

auto gse::renderer::grass::create_field_render(gpu::device& device, const std::uint32_t tile_count, field_render& fr) -> void {
	fr.tile_count = tile_count;
	fr.visible = device.create_buffer(
		{
			.size = static_cast<std::size_t>(views.grass_max_views) * tile_count * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = gpu::buffer_flag::storage,
			.writable = true,
			.device_local = true,
		},
		"grass.visible"
	);
	fr.args = device.create_buffer(
		{
			.size = static_cast<std::size_t>(views.grass_max_views) * views.grass_args_words * sizeof(std::uint32_t),
			.stride = sizeof(std::uint32_t),
			.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::indirect, gpu::buffer_flag::transfer_dst },
			.writable = true,
			.device_local = true,
		},
		"grass.draw_args"
	);
	for (auto& instance : fr.instance) {
		instance = device.create_buffer(
			{
				.size = sizeof(shaders::common::instance_data),
				.stride = sizeof(shaders::common::instance_data),
				.usage = gpu::buffer_flag::storage,
			},
			"grass.instance"
		);
	}
	fr.previous_model.reset();
	log::println(log::category::render, "Grass renderer: {} tiles, visible lists {} KiB", tile_count, fr.visible.size() / 1024);
}

auto gse::renderer::grass::args_offset(const std::uint32_t view) -> std::size_t {
	return static_cast<std::size_t>(view) * views.grass_args_words * sizeof(std::uint32_t);
}

auto gse::renderer::grass::record_cull(gpu::recording_context& rec, const field_render& field, const std::uint32_t frame_index, const cull_push_constants& pc, const std::uint32_t view_count, const gpu::bindless_slot shadow_tiles) -> void {
	rec.fill_buffer(field.args, args_offset(pc.first_view), args_offset(view_count));
	rec.dispatch<cull_entry>(
		pc,
		{
			.grass_params_in = field.params[frame_index],
			.cull_visible_out = field.visible.slot(),
			.cull_args_out = field.args.slot(),
			.cull_shadow_tiles = shadow_tiles,
		},
		vec3u{ (field.tile_count + 63) / 64, view_count, 1u }
	);
}

auto gse::renderer::grass::record_draw(gpu::recording_context& rec, const gpu::buffer& indices, const field_render& field, const std::uint32_t view) -> void {
	rec.bind_index(indices);
	rec.draw_indirect(field.args, args_offset(view), 1, views.grass_args_words * sizeof(std::uint32_t));
}

auto gse::renderer::grass::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.cull_pipeline = gpu::build_compute_program(*gpu_s.device, cull_entry::pod);

	const auto indices = build_tile_indices();
	const auto bytes = indices.size() * sizeof(std::uint32_t);
	d.index_upload = gpu_s.device->create_buffer(
		{
			.size = bytes,
			.stride = sizeof(std::uint32_t),
			.usage = { gpu::buffer_flag::storage, gpu::buffer_flag::transfer_src },
			.data = indices.data(),
		},
		"grass.index_upload"
	);
	d.indices = gpu_s.device->create_buffer(
		{
			.size = bytes,
			.stride = sizeof(std::uint32_t),
			.usage = { gpu::buffer_flag::index, gpu::buffer_flag::storage, gpu::buffer_flag::transfer_dst, gpu::buffer_flag::acceleration_structure_build_input },
			.device_local = true,
		},
		"grass.indices"
	);
	return {};
}

auto gse::renderer::grass::frame(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d, const shared_view<physics::grass::data> grass_s, const shared_view<camera::data> cam_state, const channel_write<gpu::render_pass_request> pass_out) -> async::task<> {
	std::erase_if(d.fields, [&](const auto& entry) {
		return !grass_s.buffers.contains(entry.first);
	});
	for (auto& fr : d.fields | std::views::values) {
		fr.culled = false;
	}

	if (grass_s.buffers.empty() || !gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();

	bool any = false;
	for (const auto& [owner, fb] : grass_s.buffers) {
		if (fb.tile_count == 0) {
			continue;
		}

		auto& fr = d.fields[owner];
		if (fr.tile_count != fb.tile_count) {
			create_field_render(*gpu_s.device, fb.tile_count, fr);
		}

		const spatial_matrix model(physics::transformation_matrix(fb.transform));
		fr.instance[frame_index].host_write(shaders::common::instance_data{
			.model_matrix = model,
			.normal_matrix = spatial_matrix(mat4f(mat3_cast(fb.transform.orientation))),
			.prev_model_matrix = fr.previous_model.value_or(model),
			.material_index = geometry_collector::data::grass_material_index,
			.tint = vec4f(1.f),
		});
		fr.previous_model = model;
		fr.pool = fb.nodes.slot();
		fr.tiles = fb.tiles.slot();
		fr.params[frame_index] = fb.params[frame_index].slot();
		fr.culled = true;
		any = true;
	}

	if (!any) {
		co_return;
	}

	auto rec = co_await gpu::pass<^^cull_pass>(pass_out)
		.pipeline(d.cull_pipeline)
		.after<^^physics::grass::grid_pass, ^^physics::grass::simulate_pass>();

	if (!d.indices_uploaded) {
		rec.copy_buffer(d.index_upload, d.indices, d.indices.size());
		d.indices_uploaded = true;
	}

	const cull_push_constants pc{
		.camera_view_projection = cam_state.projection_matrix * cam_state.view_matrix,
		.first_view = views.grass_camera_view,
		.shadow_views = 0,
	};
	for (const auto& fr : d.fields | std::views::values) {
		if (fr.culled) {
			record_cull(rec, fr, frame_index, pc, 1, fr.params[frame_index]);
		}
	}
}
