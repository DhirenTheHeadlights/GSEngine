export module gse.graphics:ui_renderer;

import std;

import :texture;
import :font;

import gse.platform;
import gse.utility;
import gse.math;

export namespace gse::renderer {
	struct sprite_command {
		rect_t<vec2f> rect;
		vec4f color = { 1.0f, 1.0f, 1.0f, 1.0f };
		resource::handle<texture> texture;
		vec4f uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f };
		std::optional<rect_t<vec2f>> clip_rect = std::nullopt;
		angle rotation;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
	};

	struct text_command {
		resource::handle<font> font;
		std::string text;
		vec2f position;
		float scale = 1.0f;
		vec4f color = { 1.0f, 1.0f, 1.0f, 1.0f };
		std::optional<rect_t<vec2f>> clip_rect = std::nullopt;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
	};
}

namespace gse::renderer::ui {
	enum class command_type : std::uint8_t {
		sprite,
		text
	};

	struct vertex {
		vec2f position;
		vec2f uv;
		vec4f color;
	};

	struct draw_batch {
		command_type type;
		std::uint32_t index_offset;
		std::uint32_t index_count;
		std::optional<rect_t<vec2f>> clip_rect;
		resource::handle<texture> texture;
		resource::handle<font> font;
	};

	struct frame_data {
		std::vector<vertex> vertices;
		std::vector<std::uint32_t> indices;
		std::vector<draw_batch> batches;
	};

	struct unified_command {
		command_type type;
		render_layer layer;
		std::uint32_t z_order;
		std::optional<rect_t<vec2f>> clip_rect;

		resource::handle<texture> texture;
		rect_t<vec2f> rect;
		vec4f color;
		vec4f uv_rect;
		angle rotation;

		resource::handle<font> font;
		std::string text;
		vec2f position;
		float scale;
	};

	static constexpr std::size_t max_quads_per_frame = 32768;
	static constexpr std::size_t vertices_per_quad = 4;
	static constexpr std::size_t indices_per_quad = 6;
	static constexpr std::size_t max_vertices = max_quads_per_frame * vertices_per_quad;
	static constexpr std::size_t max_indices = max_quads_per_frame * indices_per_quad;
	static constexpr std::size_t frames_in_flight = 2;

	auto to_vulkan_scissor(
		const rect_t<vec2f>& rect,
		const vec2f& window_size
	) -> vk::Rect2D;

	auto add_sprite_quad(
		std::vector<vertex>& vertices,
		std::vector<std::uint32_t>& indices,
		const unified_command& cmd
	) -> void;

	auto add_text_quads(
		std::vector<vertex>& vertices,
		std::vector<std::uint32_t>& indices,
		const unified_command& cmd
	) -> void;
}

export namespace gse::renderer::ui {
	struct frame_resources {
		gpu::buffer vertex_buffer;
		gpu::buffer index_buffer;
	};

	struct state {
		gpu::pipeline sprite_pipeline;
		resource::handle<shader> sprite_shader;

		gpu::pipeline text_pipeline;
		resource::handle<shader> text_shader;

		std::array<frame_resources, frames_in_flight> resources;
		triple_buffer<frame_data> data;

		state() = default;
	};

	struct system {
		static auto initialize(initialize_phase& phase, state& s) -> void;
		static auto update(const update_phase& phase, state& s) -> void;
		static auto render(render_phase& phase, const state& s) -> void;
	};
}

auto gse::renderer::ui::to_vulkan_scissor(const rect_t<vec2f>& rect, const vec2f& window_size) -> vk::Rect2D {
	const float left = std::max(0.0f, rect.left());
	const float right = std::min(window_size.x(), rect.right());
	const float bottom = std::max(0.0f, rect.bottom());
	const float top = std::min(window_size.y(), rect.top());

	const float width = std::max(0.0f, right - left);
	const float height = std::max(0.0f, top - bottom);

	return {
		.offset = {
			static_cast<std::int32_t>(left),
			static_cast<std::int32_t>(window_size.y() - top)
		},
		.extent = {
			static_cast<std::uint32_t>(width),
			static_cast<std::uint32_t>(height)
		}
	};
}

auto gse::renderer::ui::add_sprite_quad(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, const unified_command& cmd) -> void {
	if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
		return;
	}

	const auto base_index = static_cast<std::uint32_t>(vertices.size());

	const vec2f top_left = cmd.rect.top_left();
	const vec2f size = cmd.rect.size();
	const vec2f center = {
		top_left.x() + size.x() * 0.5f,
		top_left.y() - size.y() * 0.5f
	};

	const vec2f half = { size.x() * 0.5f, size.y() * 0.5f };
	vec2f o0 = { -half.x(),  half.y() };
	vec2f o1 = { half.x(),  half.y() };
	vec2f o2 = { half.x(), -half.y() };
	vec2f o3 = { -half.x(), -half.y() };

	o0 = rotate(o0, cmd.rotation);
	o1 = rotate(o1, cmd.rotation);
	o2 = rotate(o2, cmd.rotation);
	o3 = rotate(o3, cmd.rotation);

	const vec2f p0 = center + o0;
	const vec2f p1 = center + o1;
	const vec2f p2 = center + o2;
	const vec2f p3 = center + o3;

	const float u0 = cmd.uv_rect.x();
	const float v0 = cmd.uv_rect.y();
	const float u1 = cmd.uv_rect.x() + cmd.uv_rect.z();
	const float v1 = cmd.uv_rect.y() + cmd.uv_rect.w();

	vertices.push_back({ p0, { u0, v0 }, cmd.color });
	vertices.push_back({ p1, { u1, v0 }, cmd.color });
	vertices.push_back({ p2, { u1, v1 }, cmd.color });
	vertices.push_back({ p3, { u0, v1 }, cmd.color });

	indices.push_back(base_index + 0);
	indices.push_back(base_index + 2);
	indices.push_back(base_index + 1);
	indices.push_back(base_index + 0);
	indices.push_back(base_index + 3);
	indices.push_back(base_index + 2);
}

auto gse::renderer::ui::add_text_quads(std::vector<vertex>& vertices, std::vector<std::uint32_t>& indices, const unified_command& cmd) -> void {
	for (const auto& [screen_rect, uv_rect] : cmd.font->text_layout(cmd.text, cmd.position, cmd.scale)) {
		if (vertices.size() + 4 > max_vertices || indices.size() + 6 > max_indices) {
			break;
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

		vertices.push_back({ p0, { u0, v1 }, cmd.color });
		vertices.push_back({ p1, { u1, v1 }, cmd.color });
		vertices.push_back({ p2, { u1, v0 }, cmd.color });
		vertices.push_back({ p3, { u0, v0 }, cmd.color });

		indices.push_back(base_index + 0);
		indices.push_back(base_index + 2);
		indices.push_back(base_index + 1);
		indices.push_back(base_index + 0);
		indices.push_back(base_index + 3);
		indices.push_back(base_index + 2);
	}
}

auto gse::renderer::ui::system::initialize(initialize_phase& phase, state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	s.sprite_shader = ctx.get<shader>("Shaders/Standard2D/sprite");
	ctx.instantly_load(s.sprite_shader);

	s.sprite_pipeline = gpu::create_graphics_pipeline(ctx, *s.sprite_shader, {
		.rasterization = { .cull = gpu::cull_mode::none },
		.depth = { .test = false, .write = false },
		.blend = gpu::blend_preset::alpha_premultiplied,
		.depth_fmt = gpu::depth_format::none,
		.push_constant_block = "push_constants"
	});

	s.text_shader = ctx.get<shader>("Shaders/Standard2D/msdf");
	ctx.instantly_load(s.text_shader);

	s.text_pipeline = gpu::create_graphics_pipeline(ctx, *s.text_shader, {
		.rasterization = { .cull = gpu::cull_mode::none },
		.depth = { .test = false, .write = false },
		.blend = gpu::blend_preset::alpha_premultiplied,
		.depth_fmt = gpu::depth_format::none,
		.push_constant_block = "push_constants"
	});

	constexpr std::size_t vertex_buffer_size = max_vertices * sizeof(vertex);
	constexpr std::size_t index_buffer_size = max_indices * sizeof(std::uint32_t);

	for (auto& [vertex_buffer, index_buffer] : s.resources) {
		vertex_buffer = gpu::create_buffer(ctx, {
			.size = vertex_buffer_size,
			.usage = gpu::buffer_flag::vertex
		});

		index_buffer = gpu::create_buffer(ctx, {
			.size = index_buffer_size,
			.usage = gpu::buffer_flag::index
		});
	}

	auto& [vertices, indices, batches] = s.data.write();
	vertices.reserve(max_vertices);
	indices.reserve(max_indices);
	batches.reserve(512);
}

auto gse::renderer::ui::system::update(const update_phase& phase, state& s) -> void {
	const auto& sprite_commands = phase.read_channel<sprite_command>();
	const auto& text_commands = phase.read_channel<text_command>();

	if (sprite_commands.empty() && text_commands.empty()) {
		return;
	}

	auto& [vertices, indices, batches] = s.data.write();
	vertices.clear();
	indices.clear();
	batches.clear();

	std::vector<unified_command> unified;
	unified.reserve(sprite_commands.size() + text_commands.size());

	for (const auto& [rect, color, texture, uv_rect, clip_rect, rotation, layer, z_order] : sprite_commands) {
		if (!texture.valid()) {
			continue;
		}

		unified.push_back({
			.type = command_type::sprite,
			.layer = layer,
			.z_order = z_order,
			.clip_rect = clip_rect,
			.texture = texture,
			.rect = rect,
			.color = color,
			.uv_rect = uv_rect,
			.rotation = rotation,
			.font = {},
			.text = {},
			.position = {},
			.scale = 1.0f
		});
	}

	for (const auto& [font, text, position, scale, color, clip_rect, layer, z_order] : text_commands) {
		if (!font.valid() || text.empty()) {
			continue;
		}

		unified.push_back({
			.type = command_type::text,
			.layer = layer,
			.z_order = z_order,
			.clip_rect = clip_rect,
			.texture = {},
			.rect = {},
			.color = color,
			.uv_rect = {},
			.rotation = {},
			.font = font,
			.text = text,
			.position = position,
			.scale = scale
		});
	}

	std::ranges::stable_sort(unified, [](const unified_command& a, const unified_command& b) {
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
		} else {
			return a.font.id().number() < b.font.id().number();
		}
	});

	auto current_type = command_type::sprite;
	resource::handle<texture> current_texture;
	resource::handle<font> current_font;
	std::optional<rect_t<vec2f>> current_clip;
	std::uint32_t batch_index_start = 0;

	auto flush_batch = [&] {
		if (indices.size() > batch_index_start) {
			batches.push_back({
				.type = current_type,
				.index_offset = batch_index_start,
				.index_count = static_cast<std::uint32_t>(indices.size() - batch_index_start),
				.clip_rect = current_clip,
				.texture = current_texture,
				.font = current_font
			});
		}
		batch_index_start = static_cast<std::uint32_t>(indices.size());
	};

	for (const auto& cmd : unified) {
		bool needs_flush = false;

		if (cmd.type != current_type) {
			needs_flush = true;
		} else if (cmd.clip_rect != current_clip) {
			needs_flush = true;
		} else if (cmd.type == command_type::sprite && cmd.texture.id() != current_texture.id()) {
			needs_flush = true;
		} else if (cmd.type == command_type::text && cmd.font.id() != current_font.id()) {
			needs_flush = true;
		}

		if (needs_flush) {
			flush_batch();
			current_type = cmd.type;
			current_clip = cmd.clip_rect;
			current_texture = cmd.texture;
			current_font = cmd.font;
		}

		if (cmd.type == command_type::sprite) {
			add_sprite_quad(vertices, indices, cmd);
		} else {
			add_text_quads(vertices, indices, cmd);
		}
	}

	flush_batch();

	s.data.publish();
}

auto gse::renderer::ui::system::render(render_phase& phase, const state& s) -> void {
	auto& ctx = phase.get<gpu::context>();

	if (!ctx.graph().frame_in_progress()) {
		return;
	}

	const auto& [vertices, indices, batches] = s.data.read();

	if (batches.empty()) {
		return;
	}
	const auto frame_index = ctx.graph().current_frame();
	auto& [vertex_buffer, index_buffer] = s.resources[frame_index];

	gse::memcpy(vertex_buffer.mapped(), vertices);
	gse::memcpy(index_buffer.mapped(), indices);

	const auto ext = ctx.graph().extent();
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

	auto sprite_pc = s.sprite_shader->cache_push_block("push_constants");
	sprite_pc.set("projection", projection);

	auto text_pc = s.text_shader->cache_push_block("push_constants");
	text_pc.set("projection", projection);

	const auto sprite_binding = s.sprite_shader->binding("spriteTexture");
	const auto text_binding = s.text_shader->binding("spriteTexture");

	auto pass = ctx.graph().add_pass<ui::state>();
	pass.track(vertex_buffer.native());
	pass.track(index_buffer.native());

	const vec2u ext_size{ width, height };

	pass
		.color_output_load()
		.record([&s, &batches, frame_index, ext_size, window_size,
			sprite_pc = std::move(sprite_pc), text_pc = std::move(text_pc),
			sprite_binding, text_binding,
			&vertex_buffer, &index_buffer](vulkan::recording_context& ctx) {

			ctx.bind_vertex(vertex_buffer);
			ctx.bind_index(index_buffer);

			ctx.set_viewport(ext_size);
			ctx.set_scissor(ext_size);

			auto bound_type = command_type::sprite;
			resource::handle<texture> bound_texture;
			resource::handle<font> bound_font;
			bool first_batch = true;

			for (const auto& [type, index_offset, index_count, clip_rect, texture, font] : batches) {
				if (index_count == 0) {
					continue;
				}

				if (first_batch || type != bound_type) {
					if (type == command_type::sprite) {
						ctx.bind(s.sprite_pipeline);
						ctx.push(s.sprite_pipeline, sprite_pc);
					} else {
						ctx.bind(s.text_pipeline);
						ctx.push(s.text_pipeline, text_pc);
					}
					bound_type = type;
					bound_texture = {};
					bound_font = {};
					first_batch = false;
				}

				if (type == command_type::sprite) {
					if (texture.valid() && texture.id() != bound_texture.id() && sprite_binding) {
						const auto info = texture->descriptor_info();
						const vk::WriteDescriptorSet write{
							.dstBinding = sprite_binding->binding,
							.descriptorCount = 1,
							.descriptorType = sprite_binding->descriptorType,
							.pImageInfo = &info
						};
						ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, s.sprite_pipeline.native_layout(), 1, std::span(&write, 1));
						bound_texture = texture;
					}
				} else {
					if (font.valid() && font.id() != bound_font.id() && text_binding) {
						const auto info = font->texture()->descriptor_info();
						const vk::WriteDescriptorSet write{
							.dstBinding = text_binding->binding,
							.descriptorCount = 1,
							.descriptorType = text_binding->descriptorType,
							.pImageInfo = &info
						};
						ctx.push_descriptor(vk::PipelineBindPoint::eGraphics, s.text_pipeline.native_layout(), 1, std::span(&write, 1));
						bound_font = font;
					}
				}

				if (clip_rect) {
					const auto sc = to_vulkan_scissor(*clip_rect, window_size);
					ctx.set_scissor(sc.offset.x, sc.offset.y, sc.extent.width, sc.extent.height);
				} else {
					ctx.set_scissor(ext_size);
				}

				ctx.draw_indexed(index_count, 1, index_offset, 0, 0);
			}
		});
}
