module gse.graphics:world_text_renderer_impl;

import std;

import :world_text_renderer;
import :sdf_grid_renderer;
import :forward_renderer;
import :camera_system;
import :gui;
import :font;
import :render_targets;


import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;

namespace gse::renderer::world_text {
	struct [[= shaders::shader_struct]] world_text_vertex {
		vec3<position> position;
		vec2f tex_coord;
	};

	struct [[
		= shaders::binding<0, 1>{},
		= shaders::ssbo_readonly
	]] world_text_vertex_buffer {
		using element = world_text_vertex;
	};

	struct [[= shaders::shader_struct]] push_constants {
		vec3f color;
		std::uint32_t tex_idx;
		vec3f shadow_color;
		float shadow_offset_px;
		vec2f unit_range;
		float shadow_softness;
		float shadow_strength;
	};

	using world_text_bindings = type_pack<shaders::standard_3d::camera_ubo, world_text_vertex_buffer, shaders::bindless::textures, shaders::bindless::textures_sampler>;
	using world_text_shader_types = type_pack<world_text_vertex>;

	using entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/WorldText">,
		gpu::types<shaders::common::shader_types, world_text_shader_types>,
		gpu::bindings<world_text_bindings>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::color_targets<gpu::color_format::hdr>,
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
		data& d,
		gpu::device& device,
		std::size_t frame_index,
		std::size_t required
	) -> void;
}

auto gse::renderer::world_text::append_glyph_quad(std::vector<world_text_vertex>& vertices, const vec3<position>& tick_origin, const vec2f& pixel_top_left, const vec2f& pixel_size, const vec4f& uv_rect) -> void {
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

	vertices.push_back({ tl, { u0, v0 } });
	vertices.push_back({ tr, { u1, v0 } });
	vertices.push_back({ br, { u1, v1 } });
	vertices.push_back({ tl, { u0, v0 } });
	vertices.push_back({ br, { u1, v1 } });
	vertices.push_back({ bl, { u0, v1 } });
}

auto gse::renderer::world_text::build_labels_for_axis(std::vector<world_text_vertex>& vertices, const font& f, const float world_scale, const length major_spacing, const int max_ticks, const bool along_x) -> void {
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

auto gse::renderer::world_text::ensure_vertex_capacity(data& d, gpu::device& device, const std::size_t frame_index, const std::size_t required) -> void {
	auto& cap = d.vertex_capacities[frame_index];
	auto& buf = d.vertex_buffers[frame_index];
	if (required <= cap && buf.valid()) {
		return;
	}
	if (buf.valid()) {
		buf = {};
	}
	cap = std::max<std::size_t>(cap, 512);
	while (cap < required) {
		cap *= 2;
	}
	buf = device.create_buffer(
		{
			.size = cap * sizeof(world_text_vertex),
			.stride = sizeof(world_text_vertex),
			.usage = gpu::buffer_flag::storage,
			.bindless = true
		},
		"world_text.vertex"
	);
}

auto gse::renderer::world_text::init(context& ctx, const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.pipeline = gpu::build_graphics_program(*gpu_s.device, entry::pod);

	d.text_sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	constexpr std::size_t camera_ubo_size = sizeof(shaders::common::camera_data);

	for (std::size_t i = 0; i < per_frame_resource<gpu::buffer>::frames_in_flight; ++i) {
		d.camera_ubo_buffers[i] = gpu_s.device->create_buffer(
			{
				.size = camera_ubo_size,
				.stride = sizeof(shaders::common::camera_data),
				.usage = gpu::buffer_flag::storage,
				.bindless = true
			},
			"world_text.camera_ubo"
		);
	}

	return {};
}

auto gse::renderer::world_text::frame(const context& ctx, shared_view<gpu::context::data> gpu_s, data& d, shared_view<camera::data> cam_state, shared_view<gui::data> gui_d, shared_view<sdf_grid::data> grid_d) -> async::task<> {
	if (!grid_d.enabled || !grid_d.show_labels) {
		co_return;
	}

	if (!gpu_s.render_graph->frame_in_progress()) {
		co_return;
	}

	if (!gui_d.fonts.text.valid()) {
		co_return;
	}

	const auto font = gui_d.fonts.text.resolve();
	if (!font->texture() || !font->texture()->bindless_slot().valid()) {
		co_return;
	}

	const float world_scale = grid_d.label_size / meters(1.f);
	const int max_ticks = std::max(1, static_cast<int>(grid_d.fade_distance / grid_d.major_spacing));

	std::vector<world_text_vertex> vertices;
	vertices.reserve(static_cast<std::size_t>(max_ticks) * 32);
	build_labels_for_axis(vertices, *font, world_scale, grid_d.major_spacing, max_ticks, true);
	build_labels_for_axis(vertices, *font, world_scale, grid_d.major_spacing, max_ticks, false);

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
		.prev_view = cam_state.prev_view_matrix,
		.prev_proj = cam_state.prev_projection_matrix,
		.jitter_ndc = cam_state.jitter_ndc,
		.prev_jitter_ndc = cam_state.prev_jitter_ndc,
	};
	d.camera_ubo_buffers[frame_index].host_write(camera);

	const auto atlas_size = font->texture()->image_data().size;
	const float atlas_w = std::max(static_cast<float>(atlas_size.x()), 1.f);
	const float atlas_h = std::max(static_cast<float>(atlas_size.y()), 1.f);
	const vec2f unit_range{ font->pixel_range() / atlas_w, font->pixel_range() / atlas_h };

	const auto ext = gpu_s.render_graph->extent();
	const auto vertex_count = static_cast<std::uint32_t>(vertices.size());

	auto rec = co_await gpu::pass<^^gse::renderer::world_text::frame>(ctx)
		.pipeline(d.pipeline)
		.color(gpu::load_color(gpu_s.render_graph->framebuffer_image<targets::hdr_color>()))
		.depth(gpu::load_depth())
		.after<^^sdf_grid::frame>();

	rec.set_viewport(ext);
	rec.set_scissor(ext);
	rec.push_bindings<entry>(
		{
			.color = grid_d.label_color,
			.tex_idx = font->texture()->bindless_slot().index,
			.shadow_color = vec3f{ 0.f, 0.f, 0.f },
			.shadow_offset_px = 1.5f,
			.unit_range = unit_range,
			.shadow_softness = 0.6f,
			.shadow_strength = 0.55f,
		},
		{
			.camera_ubo = d.camera_ubo_buffers[frame_index].slot(),
			.world_text_vertex_buffer = d.vertex_buffers[frame_index].slot(),
			.textures_sampler = d.text_sampler.slot(),
		}
	);
	rec.draw(vertex_count);
}
