module gse.graphics;

import std;

import :world_text_renderer;
import :sdf_grid_renderer;
import :forward_renderer;
import :camera_system;
import :gui;
import :font;
import :render_targets;

import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::world_text {
	struct [[= shaders::shader_struct]] push_constants {
		vec3f color;
		vec2f unit_range;
		std::uint32_t tex_idx;
		vec3f shadow_color;
		float shadow_offset_px;
		float shadow_softness;
		float shadow_strength;
	};

	struct world_text_vertex {
		vec3<position> position;
		vec2f tex_coord;
	};

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/WorldText">,
		gpu::layout<"standard_3d">,
		gpu::types<shaders::common::shader_types>,
		gpu::bindings<shaders::standard_3d::shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_target<gpu::color_format::hdr>,
		gpu::depth<true, false, gpu::compare_op::less_or_equal>,
		gpu::blend<gpu::blend_preset::alpha_premultiplied>
	>;

	auto append_glyph_quad(
		std::vector<world_text_vertex>& vertices,
		const vec3<position>& tick_origin,
		const vec2f& pixel_top_left,
		const vec2f& pixel_size,
		const vec4f& uv_rect
	) -> void;

	auto build_labels_for_axis(
		std::vector<world_text_vertex>& vertices,
		const font& f,
		float world_scale,
		length major_spacing,
		int max_ticks,
		bool along_x
	) -> void;

	auto ensure_vertex_capacity(
		system::data& d,
		gpu::device& device,
		std::size_t frame_index,
		std::size_t required
	) -> void;
}

auto gse::renderer::world_text::append_glyph_quad(
	std::vector<world_text_vertex>& vertices,
	const vec3<position>& tick_origin,
	const vec2f& pixel_top_left,
	const vec2f& pixel_size,
	const vec4f& uv_rect
) -> void {
	const length lift = meters(0.001f);
	const length x0 = meters(pixel_top_left.x());
	const length x1 = meters(pixel_top_left.x() + pixel_size.x());
	const length z0 = meters(-pixel_top_left.y());
	const length z1 = meters(-(pixel_top_left.y() - pixel_size.y()));

	const vec3<position> tl = tick_origin + vec3<length>(x0, lift, z0);
	const vec3<position> tr = tick_origin + vec3<length>(x1, lift, z0);
	const vec3<position> br = tick_origin + vec3<length>(x1, lift, z1);
	const vec3<position> bl = tick_origin + vec3<length>(x0, lift, z1);

	const float u0 = uv_rect.x();
	const float v0 = uv_rect.y();
	const float u1 = uv_rect.x() + uv_rect.z();
	const float v1 = uv_rect.y() + uv_rect.w();

	vertices.push_back({ tl, { u0, v1 } });
	vertices.push_back({ tr, { u1, v1 } });
	vertices.push_back({ br, { u1, v0 } });
	vertices.push_back({ tl, { u0, v1 } });
	vertices.push_back({ br, { u1, v0 } });
	vertices.push_back({ bl, { u0, v0 } });
}

auto gse::renderer::world_text::build_labels_for_axis(
	std::vector<world_text_vertex>& vertices,
	const font& f,
	const float world_scale,
	const length major_spacing,
	const int max_ticks,
	const bool along_x
) -> void {
	for (int n = -max_ticks; n <= max_ticks; ++n) {
		if (n == 0) {
			continue;
		}
		const length offset = major_spacing * static_cast<float>(n);
		const auto text = std::format("{:.0f}", offset);
		const float text_width = f.width(text, world_scale);
		const vec2f start{ -text_width * 0.5f, f.line_height(world_scale) * 0.5f };

		const vec3<position> tick = along_x ? vec3<position>(offset, meters(0.f), meters(0.f))
											: vec3<position>(meters(0.f), meters(0.f), offset);

		for (const auto& glyph : f.text_layout(text, start, world_scale)) {
			append_glyph_quad(vertices, tick, glyph.screen_rect.top_left(), glyph.screen_rect.size(), glyph.uv_rect);
		}
	}
}

auto gse::renderer::world_text::ensure_vertex_capacity(
	system::data& d,
	gpu::device& device,
	const std::size_t frame_index,
	const std::size_t required
) -> void {
	auto& cap = d.vertex_capacities[frame_index];
	auto& buf = d.vertex_buffers[frame_index];
	if (required <= cap && buf) {
		return;
	}
	if (buf) {
		buf = {};
	}
	cap = std::max<std::size_t>(cap, 512);
	while (cap < required) {
		cap *= 2;
	}
	buf = gpu::buffer::create(
		device.allocator(),
		{
			.size = cap * sizeof(world_text_vertex),
			.usage = gpu::buffer_flag::vertex
		}
	);
}

auto gse::renderer::world_text::system::run(
	run_context& ctx,
	const gpu::context::data& gpu_s,
	data& d
) -> async::task<> {
	d.pipeline =
		gpu::build_graphics_pipeline(*gpu_s.device, *gpu_s.shader_registry, *gpu_s.bindless_textures, entry::pod);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::descriptor_region>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu::buffer::create(
			gpu_s.device->allocator(),
			{
				.size = camera_ubo_size,
				.usage = gpu::buffer_flag::uniform
			}
		);
		d.descriptors[i] =
			gpu::allocate_descriptors(*gpu_s.shader_registry, gpu_s.device->descriptor_heap(), entry::pod);

		gpu::descriptor_writer(gpu::context::device_handle(*gpu_s.device), d.descriptors[i])
			.buffer<shaders::standard_3d::camera_ubo>(d.camera_ubo_buffers[i], 0, camera_ubo_size)
			.commit();
	}

	co_return;
}

auto gse::renderer::world_text::system::frame(
	const frame_context& ctx,
	shared_view<gpu::context> gpu_s,
	data& d,
	shared_view<camera::system> cam_state,
	shared_view<gui::system> gui_d,
	shared_view<sdf_grid::system> grid_d
) -> async::task<> {
	if (!grid_d.enabled || !grid_d.show_labels) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!gui_d.gui_font.valid()) {
		co_return;
	}

	const auto& f = gui_d.gui_font.resolve();
	if (!f.texture() || !f.texture()->bindless_slot()) {
		co_return;
	}

	const float world_scale = grid_d.label_size / meters(1.f);
	const int max_ticks = std::max(1, static_cast<int>(grid_d.fade_distance / grid_d.major_spacing));

	std::vector<world_text_vertex> vertices;
	vertices.reserve(static_cast<std::size_t>(max_ticks) * 32);
	build_labels_for_axis(vertices, f, world_scale, grid_d.major_spacing, max_ticks, true);
	build_labels_for_axis(vertices, f, world_scale, grid_d.major_spacing, max_ticks, false);

	if (vertices.empty()) {
		co_return;
	}

	const auto frame_index = gpu_s.render_graph->current_frame();
	ensure_vertex_capacity(d, *gpu_s.device, frame_index, vertices.size());
	d.vertex_buffers[frame_index].host_write(vertices);

	const auto view = cam_state.view_matrix;
	const auto proj = cam_state.projection_matrix;
	const shaders::common::camera_data camera{
		.view = view,
		.proj = proj,
		.inv_view = view.inverse(),
		.inv_view_proj = (proj * view).inverse(),
	};
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const auto atlas_size = f.texture()->image_data().size;
	const float atlas_w = std::max(static_cast<float>(atlas_size.x()), 1.f);
	const float atlas_h = std::max(static_cast<float>(atlas_size.y()), 1.f);
	const vec2f unit_range{ f.pixel_range() / atlas_w, f.pixel_range() / atlas_h };

	const gpu::typed_push_constants<push_constants> pc{
		.data = {
			.color = grid_d.label_color,
			.unit_range = unit_range,
			.tex_idx = f.texture()->bindless_slot().index,
			.shadow_color = vec3f{ 0.f, 0.f, 0.f },
			.shadow_offset_px = 1.5f,
			.shadow_softness = 0.6f,
			.shadow_strength = 0.55f,
		},
		.stages = gpu::stage_flag::vertex | gpu::stage_flag::fragment,
	};

	const auto ext = gpu_s.render_graph->extent();
	const auto vertex_count = static_cast<std::uint32_t>(vertices.size());

	auto rec = co_await gpu::pass<system>(ctx)
				   .pipeline(d.pipeline)
				   .color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
				   .depth(gpu::load_depth())
				   .after<sdf_grid::system>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.bind_descriptors(d.pipeline, d.descriptors[frame_index]);
	rec.push(d.pipeline, pc);
	rec.bind_vertex(d.vertex_buffers[frame_index]);
	rec.draw(vertex_count);
}
