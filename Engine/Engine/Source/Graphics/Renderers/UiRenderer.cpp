module gse.graphics:ui_renderer_impl;

import std;

import :ui_renderer;
import :texture;
import :font;
import :forward_renderer;
import :scene_snapshot_renderer;
import :physics_debug_renderer;
import :sdf_grid_renderer;
import :tonemap_renderer;
import :world_text_renderer;


import gse.os;
import gse.assets;
import gse.gpu;
import gse.gpu_record;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.meta;
import gse.log;

namespace gse::renderer::ui {
	struct record_stats {
		vec2u extent;
		std::size_t batches = 0;
		std::size_t vertices = 0;
		std::size_t indices = 0;
		std::size_t dropped_quads = 0;
		std::size_t dropped_batches = 0;
	};

	auto note_record_state(data& d, const record_state state, const record_stats& stats) -> void {
		const bool changed = state != d.last_record_state || stats.extent.x() != d.last_extent.x() || stats.extent.y() != d.last_extent.y();
		if (!changed) {
			++d.frames_since_state_change;
			return;
		}
		const record_state_info from = annotation_from_enum<record_state_info>(d.last_record_state, {});
		const record_state_info to = annotation_from_enum<record_state_info>(state, {});
		log::println(
			log::category::render,
			"[ui] record state {} -> {} after {} frames: extent={}x{} batches={} verts={} indices={} dropped_quads={}/{} dropped_batches={}/{} published={} recorded={}",
			from.label,
			to.label,
			d.frames_since_state_change,
			stats.extent.x(),
			stats.extent.y(),
			stats.batches,
			stats.vertices,
			stats.indices,
			stats.dropped_quads,
			max_quads_per_frame,
			stats.dropped_batches,
			max_batches_per_frame,
			d.published_frames,
			d.recorded_frames
		);
		d.last_record_state = state;
		d.last_extent = stats.extent;
		d.frames_since_state_change = 0;
	}

	struct [[
		= shaders::binding<0, 0>{},
		= shaders::ssbo_readonly
	]] vertex_buffer {
		using element = vertex;
	};

	using shader_binding_types = type_pack<vertex_buffer, shaders::bindless::textures, shaders::bindless::textures_sampler>;

	using shader_types = type_pack<vertex>;

	struct [[= shaders::shader_struct]] sprite_push_constants {
		projection_matrix projection;
		std::uint32_t tex_idx;
		std::uint32_t snapshot_tex_idx;
		vec2f inv_screen_size;
	};

	using sprite_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/sprite">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<sprite_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::depth_target<gpu::depth_format::none>
	>;

	struct [[= shaders::shader_struct]] msdf_push_constants {
		projection_matrix projection;
		vec2f unit_range;
		float depth;
		std::uint32_t tex_idx;
		vec3f shadow_color;
		float shadow_offset_px;
		float shadow_softness;
		float shadow_strength;
	};

	using msdf_entry = gpu::graphics_entry<
		gpu::body_path<"Graphics/msdf">,
		gpu::types<shader_types>,
		gpu::bindings<shader_binding_types>,
		gpu::vertex_stage<"vs_main">,
		gpu::fragment_stage<"fs_main">,
		gpu::push_constant<msdf_push_constants>,
		gpu::rasterization<gpu::polygon_mode::fill, gpu::cull_mode::none>,
		gpu::depth<false, false>,
		gpu::blend<gpu::blend_preset::alpha>,
		gpu::depth_target<gpu::depth_format::none>
	>;
}

auto gse::renderer::ui::add_sprite_quad(linear_vector<vertex>& vertices, linear_vector<std::uint32_t>& indices, const unified_command& cmd) -> std::size_t {
	if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
		return 1;
	}

	const auto base_index = static_cast<std::uint32_t>(vertices.size());

	const vec2f top_left = cmd.rect.top_left();
	const vec2f size = cmd.rect.size();
	const vec2f center = { top_left.x() + size.x() * 0.5f, top_left.y() - size.y() * 0.5f };

	const vec2f half = { size.x() * 0.5f, size.y() * 0.5f };
	const vec2f l0 = { -half.x(), half.y() };
	const vec2f l1 = { half.x(), half.y() };
	const vec2f l2 = { half.x(), -half.y() };
	const vec2f l3 = { -half.x(), -half.y() };

	const vec2f p0 = center + rotate(l0, cmd.rotation);
	const vec2f p1 = center + rotate(l1, cmd.rotation);
	const vec2f p2 = center + rotate(l2, cmd.rotation);
	const vec2f p3 = center + rotate(l3, cmd.rotation);

	const float u0 = cmd.uv_rect.x();
	const float v0 = cmd.uv_rect.y();
	const float u1 = cmd.uv_rect.x() + cmd.uv_rect.z();
	const float v1 = cmd.uv_rect.y() + cmd.uv_rect.w();

	const auto shape_kind = static_cast<std::uint32_t>(cmd.shape);

	vertices.push_back(vertex{ p0, { u0, v0 }, cmd.color, l0, half, cmd.corner_radius, shape_kind, cmd.arc_radius, cmd.arc_half_sweep, cmd.arc_thickness });
	vertices.push_back(vertex{ p1, { u1, v0 }, cmd.color, l1, half, cmd.corner_radius, shape_kind, cmd.arc_radius, cmd.arc_half_sweep, cmd.arc_thickness });
	vertices.push_back(vertex{ p2, { u1, v1 }, cmd.color, l2, half, cmd.corner_radius, shape_kind, cmd.arc_radius, cmd.arc_half_sweep, cmd.arc_thickness });
	vertices.push_back(vertex{ p3, { u0, v1 }, cmd.color, l3, half, cmd.corner_radius, shape_kind, cmd.arc_radius, cmd.arc_half_sweep, cmd.arc_thickness });

	indices.push_back(base_index + 0);
	indices.push_back(base_index + 2);
	indices.push_back(base_index + 1);
	indices.push_back(base_index + 0);
	indices.push_back(base_index + 3);
	indices.push_back(base_index + 2);

	return 0;
}

auto gse::renderer::ui::add_text_quads(linear_vector<vertex>& vertices, linear_vector<std::uint32_t>& indices, const unified_command& cmd) -> std::size_t {
	if (!cmd.font.valid()) {
		return 0;
	}

	std::size_t dropped = 0;
	const auto font_view = cmd.font.resolve();
	for (const auto& [screen_rect, uv_rect] : font_view->text_layout(cmd.text, cmd.position, cmd.scale)) {
		if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
			++dropped;
			continue;
		}

		const auto base_index = static_cast<std::uint32_t>(vertices.size());

		const vec2f top_left = screen_rect.top_left();
		const vec2f sz = screen_rect.size();

		const vec2f p0 = top_left;
		const vec2f p1 = { top_left.x() + sz.x(), top_left.y() };
		const vec2f p2 = { top_left.x() + sz.x(), top_left.y() - sz.y() };
		const vec2f p3 = { top_left.x(), top_left.y() - sz.y() };

		const float u0 = uv_rect.x();
		const float v0 = uv_rect.y();
		const float u1 = uv_rect.x() + uv_rect.z();
		const float v1 = uv_rect.y() + uv_rect.w();

		vertices.push_back({ p0, { u0, v0 }, cmd.color, {}, {}, 0.f });
		vertices.push_back({ p1, { u1, v0 }, cmd.color, {}, {}, 0.f });
		vertices.push_back({ p2, { u1, v1 }, cmd.color, {}, {}, 0.f });
		vertices.push_back({ p3, { u0, v1 }, cmd.color, {}, {}, 0.f });

		indices.push_back(base_index + 0);
		indices.push_back(base_index + 2);
		indices.push_back(base_index + 1);
		indices.push_back(base_index + 0);
		indices.push_back(base_index + 3);
		indices.push_back(base_index + 2);
	}

	return dropped;
}

auto gse::renderer::ui::init(const shared_view<gpu::context::data> gpu_s, data& d) -> async::task<> {
	d.sprite_pipeline = gpu::build_graphics_program(*gpu_s.device, sprite_entry::pod);
	d.text_pipeline = gpu::build_graphics_program(*gpu_s.device, msdf_entry::pod);

	d.ui_sampler = gpu_s.device->register_sampler(
		{
			.min = gpu::sampler_filter::linear,
			.mag = gpu::sampler_filter::linear,
			.address_u = gpu::sampler_address_mode::clamp_to_edge,
			.address_v = gpu::sampler_address_mode::clamp_to_edge,
			.address_w = gpu::sampler_address_mode::clamp_to_edge,
		}
	);

	constexpr std::size_t vertex_buffer_size = max_vertices * sizeof(vertex);
	constexpr std::size_t index_buffer_size = max_indices * sizeof(std::uint32_t);

	for (auto& [vertex_buffer, index_buffer] : d.gpu_frames) {
		vertex_buffer = gpu_s.device->create_buffer(
			{
				.size = vertex_buffer_size,
				.stride = sizeof(vertex),
				.usage = gpu::buffer_flag::storage,
				.bindless = true,
			}
		);

		index_buffer = gpu_s.device->create_buffer(
			{
				.size = index_buffer_size,
				.usage = gpu::buffer_flag::index,
			}
		);
	}

	return {};
}

auto gse::renderer::ui::run(context& ctx, const shared_view<gpu::context::data> gpu_s, const shared_view<asset::data> assets_s, data& d, const channel_read<sprite_command, text_command> commands_in) -> async::task<> {
	const auto& sprite_commands = commands_in.of<sprite_command>();
	const auto& text_commands = commands_in.of<text_command>();

	if (sprite_commands.empty() && text_commands.empty()) {
		return {};
	}

	auto& [vertices, indices, batches, dropped_quads, dropped_batches] = d.buffered_frames.write();
	vertices.clear();
	indices.clear();
	batches.clear();
	dropped_quads = 0;
	dropped_batches = 0;

	std::vector<unified_command> unified;
	unified.reserve(sprite_commands.size() + text_commands.size());

	for (const auto& cmd : sprite_commands) {
		if (!cmd.texture.valid() && !cmd.image_slot.valid()) {
			continue;
		}

		unified.push_back({
			.type = command_type::sprite,
			.window = cmd.window,
			.layer = cmd.layer,
			.z_order = cmd.z_order,
			.clip_rect = cmd.clip_rect,
			.texture = cmd.texture,
			.rect = cmd.rect,
			.color = cmd.color,
			.uv_rect = cmd.uv_rect,
			.rotation = cmd.rotation,
			.corner_radius = cmd.corner_radius,
			.shape = cmd.shape,
			.arc_radius = cmd.arc_radius,
			.arc_half_sweep = cmd.arc_half_sweep,
			.arc_thickness = cmd.arc_thickness,
			.sample_scene_snapshot = cmd.sample_scene_snapshot,
			.image_slot = cmd.image_slot,
			.font = {},
			.text = {},
			.position = {},
			.scale = 1.0f,
		});
	}

	for (const auto& cmd : text_commands) {
		if (!cmd.font.valid() || cmd.text.empty()) {
			continue;
		}

		unified.push_back({
			.type = command_type::text,
			.window = cmd.window,
			.layer = cmd.layer,
			.z_order = cmd.z_order,
			.clip_rect = cmd.clip_rect,
			.texture = {},
			.rect = {},
			.color = cmd.color,
			.uv_rect = {},
			.rotation = {},
			.font = cmd.font,
			.text = cmd.text,
			.position = cmd.position,
			.scale = cmd.scale,
		});
	}

	std::ranges::stable_sort(
		unified,
		[](const unified_command& a, const unified_command& b) {
			if (a.window != b.window) {
				return a.window.number() < b.window.number();
			}

			if (a.layer != b.layer) {
				return static_cast<std::uint8_t>(a.layer) < static_cast<std::uint8_t>(b.layer);
			}

			if (a.z_order != b.z_order) {
				return a.z_order < b.z_order;
			}

			if (a.type != b.type) {
				return a.type < b.type;
			}

			if (a.type == command_type::sprite) {
				return a.texture.id().number() < b.texture.id().number();
			}
			return a.font.id().number() < b.font.id().number();
		}
	);

	auto current_type = command_type::sprite;
	gse::id current_window;
	resource::handle<texture> current_texture;
	resource::handle<font> current_font;
	std::optional<rect_t<vec2f>> current_clip;
	bool current_sample_snapshot = false;
	gpu::bindless_slot current_image_slot = {};
	std::uint32_t batch_index_start = 0;

	auto flush_batch = [&] {
		if (indices.size() > batch_index_start) {
			if (batches.size() == max_batches_per_frame) {
				++dropped_batches;
			}
			else {
				batches.push_back({
					.type = current_type,
					.window = current_window,
					.index_offset = batch_index_start,
					.index_count = static_cast<std::uint32_t>(indices.size() - batch_index_start),
					.clip_rect = current_clip,
					.texture = current_texture,
					.font = current_font,
					.sample_scene_snapshot = current_sample_snapshot,
					.image_slot = current_image_slot,
				});
			}
		}
		batch_index_start = static_cast<std::uint32_t>(indices.size());
	};

	for (const auto& cmd : unified) {
		bool needs_flush = false;

		if (cmd.window != current_window) {
			needs_flush = true;
		}
		else if (cmd.type != current_type) {
			needs_flush = true;
		}
		else if (cmd.clip_rect.has_value() != current_clip.has_value()) {
			needs_flush = true;
		}
		else if (cmd.clip_rect.has_value() && (cmd.clip_rect->min() != current_clip->min() || cmd.clip_rect->max() != current_clip->max())) {
			needs_flush = true;
		}
		else if (cmd.type == command_type::sprite && cmd.texture.id() != current_texture.id()) {
			needs_flush = true;
		}
		else if (cmd.type == command_type::text && cmd.font.id() != current_font.id()) {
			needs_flush = true;
		}
		else if (cmd.type == command_type::sprite && cmd.sample_scene_snapshot != current_sample_snapshot) {
			needs_flush = true;
		}
		else if (cmd.type == command_type::sprite && cmd.image_slot.index != current_image_slot.index) {
			needs_flush = true;
		}

		if (needs_flush) {
			flush_batch();
			current_window = cmd.window;
			current_type = cmd.type;
			current_clip = cmd.clip_rect;
			current_texture = cmd.texture;
			current_font = cmd.font;
			current_sample_snapshot = cmd.sample_scene_snapshot;
			current_image_slot = cmd.image_slot;
		}

		if (cmd.type == command_type::sprite) {
			dropped_quads += add_sprite_quad(vertices, indices, cmd);
		}
		else {
			dropped_quads += add_text_quads(vertices, indices, cmd);
		}
	}

	flush_batch();

	d.buffered_frames.publish();
	++d.published_frames;

	return {};
}

auto gse::renderer::ui::frame(context& ctx, shared_view<gpu::context::data> gpu_s, data& d, const channel_write<gpu::render_pass_request> pass_out, shared_view<scene_snapshot::data> snapshot_s) -> async::task<> {
	if (!gpu_s.render_graph->frame_in_progress()) {
		note_record_state(d, record_state::skipped_no_frame, { .extent = gpu_s.render_graph->extent() });
		co_return;
	}

	const auto& [vertices, indices, batches, dropped_quads, dropped_batches] = d.buffered_frames.read();

	if (batches.empty()) {
		note_record_state(d, record_state::skipped_no_batches, {
			.extent = gpu_s.render_graph->extent(),
			.vertices = vertices.size(),
			.indices = indices.size(),
		});
		co_return;
	}

	const bool truncated = dropped_quads > 0 || dropped_batches > 0;
	note_record_state(d, truncated ? record_state::truncated : record_state::recording, {
		.extent = gpu_s.render_graph->extent(),
		.batches = batches.size(),
		.vertices = vertices.size(),
		.indices = indices.size(),
		.dropped_quads = dropped_quads,
		.dropped_batches = dropped_batches,
	});
	++d.recorded_frames;

	const auto frame_index = gpu_s.render_graph->current_frame();
	auto& [vertex_buffer, index_buffer] = d.gpu_frames[frame_index];

	vertex_buffer.host_write(vertices);
	index_buffer.host_write(indices);

	std::vector<gse::id> windows;
	for (const auto& b : batches) {
		if (std::ranges::find(windows, b.window) == windows.end()) {
			windows.push_back(b.window);
		}
	}

	for (const gse::id window : windows) {
		const auto ext = gpu_s.render_graph->extent(window);
		if (ext.x() == 0 || ext.y() == 0) {
			continue;
		}
		const auto width = ext.x();
		const auto height = ext.y();
		const vec2f window_size = { static_cast<float>(width), static_cast<float>(height) };

		const auto projection = orthographic(
			meters(0.0f),
			meters(static_cast<float>(width)),
			meters(0.0f),
			meters(static_cast<float>(height)),
			meters(-1.0f),
			meters(1.0f)
		);

		const vec2f inv_screen_size{ width > 0 ? 1.0f / static_cast<float>(width) : 0.0f,
									 height > 0 ? 1.0f / static_cast<float>(height) : 0.0f };

		const std::uint32_t snapshot_idx = [&]() -> std::uint32_t {
			if (!snapshot_s.ready) {
				return shaders::bindless::invalid_index;
			}
			const auto& slot = snapshot_s.slots[frame_index];
			return slot.valid() ? slot.slot().index : shaders::bindless::invalid_index;
		}();

		sprite_push_constants sprite_pc{
			.projection = projection,
			.tex_idx = 0,
			.snapshot_tex_idx = shaders::bindless::invalid_index,
			.inv_screen_size = inv_screen_size,
		};

		msdf_push_constants text_pc{
			.projection = projection,
			.unit_range = {},
			.depth = 0.0f,
			.tex_idx = 0,
			.shadow_color = vec3f{ 0.f, 0.f, 0.f },
			.shadow_offset_px = 1.0f,
			.shadow_softness = 0.7f,
			.shadow_strength = 0.45f,
		};

		const vec2u ext_size{ width, height };

		auto builder = window.exists()
			? gpu::pass(pass_out, find_or_generate_id(std::format("gse::renderer::ui::frame#{}", window.number())))
			: gpu::pass<^^gse::renderer::ui::frame>(pass_out);

		auto rec = co_await std::move(builder)
			.color(gpu::load_color())
			.target(window)
			.after<^^forward::frame, ^^scene_snapshot::frame, ^^physics_debug::frame, ^^sdf_grid::frame, ^^tonemap::frame, ^^world_text::frame>();

		if (snapshot_idx != shaders::bindless::invalid_index) {
			rec.sample_image(snapshot_s.snapshots[frame_index], gpu::pipeline_stage_flag::fragment_shader);
		}

		rec.bind_index(index_buffer);

		rec.set_viewport(ext_size);
		rec.set_scissor(ext_size);

		auto bound_type = command_type::sprite;
		bool first_batch = true;

		for (const auto& batch : batches) {
			if (batch.window != window || batch.index_count == 0) {
				continue;
			}
			const auto type = batch.type;
			const auto index_offset = batch.index_offset;
			const auto index_count = batch.index_count;
			const auto& clip_rect = batch.clip_rect;
			const auto& texture = batch.texture;
			const auto& font = batch.font;
			const auto sample_scene_snapshot = batch.sample_scene_snapshot;
			const auto image_slot = batch.image_slot;

			if (first_batch || type != bound_type) {
				if (type == command_type::sprite) {
					rec.bind(d.sprite_pipeline);
				}
				else {
					rec.bind(d.text_pipeline);
				}
				bound_type = type;
				first_batch = false;
			}

			std::uint32_t tex_idx = 0;
			bool has_texture = false;
			const auto texture_view = texture.valid() ? texture.resolve() : nullptr;
			const auto font_view = font.valid() ? font.resolve() : nullptr;
			if (type == command_type::sprite) {
				if (image_slot.valid()) {
					tex_idx = image_slot.index;
					has_texture = true;
				}
				else if (texture_view && texture_view->bindless_slot().valid()) {
					tex_idx = texture_view->bindless_slot().index;
					has_texture = true;
				}
			}
			else {
				if (font_view && font_view->texture()->bindless_slot().valid()) {
					tex_idx = font_view->texture()->bindless_slot().index;
					has_texture = true;
				}
			}

			if (!has_texture) {
				continue;
			}

			if (type == command_type::sprite) {
				sprite_pc.tex_idx = tex_idx;
				sprite_pc.snapshot_tex_idx = sample_scene_snapshot ? snapshot_idx : shaders::bindless::invalid_index;
				rec.push_bindings<sprite_entry>(
					sprite_pc,
					{
						.vertex_buffer = vertex_buffer.slot(),
						.textures_sampler = d.ui_sampler.slot(),
					}
				);
			}
			else {
				const auto atlas_size = font_view->texture()->image_data().size;
				const float atlas_w = std::max(static_cast<float>(atlas_size.x()), 1.f);
				const float atlas_h = std::max(static_cast<float>(atlas_size.y()), 1.f);
				text_pc.unit_range = { font_view->pixel_range() / atlas_w, font_view->pixel_range() / atlas_h };
				text_pc.tex_idx = tex_idx;
				rec.push_bindings<msdf_entry>(
					text_pc,
					{
						.vertex_buffer = vertex_buffer.slot(),
						.textures_sampler = d.ui_sampler.slot(),
					}
				);
			}

			if (clip_rect) {
				const float left = std::max(0.0f, clip_rect->left());
				const float right = std::min(window_size.x(), clip_rect->right());
				const float bottom = std::max(0.0f, clip_rect->bottom());
				const float top = std::min(window_size.y(), clip_rect->top());
				rec.set_scissor(
					static_cast<std::int32_t>(left),
					static_cast<std::int32_t>(window_size.y() - top),
					static_cast<std::uint32_t>(std::max(0.0f, right - left)),
					static_cast<std::uint32_t>(std::max(0.0f, top - bottom))
				);
			}
			else {
				rec.set_scissor(ext_size);
			}

			rec.draw_indexed(index_count, 1, index_offset, 0, 0);
		}
	}
}
