export module gse.graphics:ui_renderer;

import std;

import :texture;
import :font;
import :render_layer;
import :scene_snapshot_renderer;

import gse.os;
import gse.assets;
import gse.gpu;
import gse.core;
import gse.containers;
import gse.concurrency;
import gse.ecs;
import gse.math;
import gse.gpu_record;

export namespace gse::renderer {
	enum class sprite_shape : std::uint8_t {
		rect,
		arc
	};

	struct sprite_command {
		rect_t<vec2f> rect;
		vec4f color = { 1.0f, 1.0f, 1.0f, 1.0f };
		resource::handle<texture> texture;
		vec4f uv_rect = { 0.0f, 0.0f, 1.0f, 1.0f };
		std::optional<rect_t<vec2f>> clip_rect = std::nullopt;
		angle rotation;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
		float corner_radius = 0.f;
		sprite_shape shape = sprite_shape::rect;
		float arc_radius = 0.f;
		angle arc_half_sweep;
		float arc_thickness = 0.f;
		bool sample_scene_snapshot = false;
		gpu::bindless_slot image_slot = {};
		gse::id window;
	};

	struct text_command {
		resource::handle<font> font;
		std::string_view text;
		vec2f position;
		float scale = 1.0f;
		vec4f color = { 1.0f, 1.0f, 1.0f, 1.0f };
		std::optional<rect_t<vec2f>> clip_rect = std::nullopt;
		render_layer layer = render_layer::content;
		std::uint32_t z_order = 0;
		gse::id window;
	};
}

namespace gse::renderer::ui {
	enum class command_type : std::uint8_t {
		sprite,
		text
	};

	struct [[= shaders::shader_struct]] vertex {
		vec2f position;
		vec2f uv;
		vec4f color;
		vec2f local_pos;
		vec2f half_size;
		float corner_radius = 0.f;
		std::uint32_t shape_kind = 0;
		float arc_radius = 0.f;
		angle arc_half_sweep;
		float arc_thickness = 0.f;
	};

	struct draw_batch {
		command_type type;
		gse::id window;
		std::uint32_t index_offset;
		std::uint32_t index_count;
		std::optional<rect_t<vec2f>> clip_rect;
		resource::handle<texture> texture;
		resource::handle<font> font;
		bool sample_scene_snapshot = false;
		gpu::bindless_slot image_slot = {};
	};

	export constexpr std::size_t max_quads_per_frame = 32768;
	export constexpr std::size_t vertices_per_quad = 4;
	export constexpr std::size_t indices_per_quad = 6;
	export constexpr std::size_t max_vertices = max_quads_per_frame * vertices_per_quad;
	export constexpr std::size_t max_indices = max_quads_per_frame * indices_per_quad;
	export constexpr std::size_t frames_in_flight = 2;

	struct gpu_frame_data {
		linear_vector<vertex> vertices{ max_vertices };
		linear_vector<std::uint32_t> indices{ max_indices };
		linear_vector<draw_batch> batches{ 512 };
	};

	struct unified_command {
		command_type type;
		gse::id window;
		render_layer layer;
		std::uint32_t z_order;
		std::optional<rect_t<vec2f>> clip_rect;

		resource::handle<texture> texture;
		rect_t<vec2f> rect;
		vec4f color;
		vec4f uv_rect;
		angle rotation;
		float corner_radius = 0.f;
		sprite_shape shape = sprite_shape::rect;
		float arc_radius = 0.f;
		angle arc_half_sweep;
		float arc_thickness = 0.f;
		bool sample_scene_snapshot = false;
		gpu::bindless_slot image_slot = {};

		resource::handle<font> font;
		std::string_view text;
		vec2f position;
		float scale;
	};

	auto add_sprite_quad(
		linear_vector<vertex>& vertices,
		linear_vector<std::uint32_t>& indices,
		const unified_command& cmd
	) -> void;

	auto add_text_quads(
		linear_vector<vertex>& vertices,
		linear_vector<std::uint32_t>& indices,
		const unified_command& cmd
	) -> void;
}

export namespace gse::renderer::ui {
	struct frame_resources {
		gpu::buffer vertex_buffer;
		gpu::buffer index_buffer;
	};

	enum class record_state : std::uint8_t {
		unknown,
		recording,
		skipped_no_frame,
		skipped_no_batches
	};

	struct [[= gse::system_state<"Ui">{}]] data {
		gpu::shader_program sprite_pipeline;
		gpu::shader_program text_pipeline;
		gpu::bindless_handle ui_sampler;
		std::array<frame_resources, frames_in_flight> gpu_frames;

		triple_buffer<gpu_frame_data> buffered_frames;

		record_state last_record_state = record_state::unknown;
		vec2u last_extent;
		std::uint64_t frames_since_state_change = 0;
		std::uint64_t published_frames = 0;
		std::uint64_t recorded_frames = 0;
	};

	[[= gse::system_init{}]]
	auto init(
		shared_view<gpu::context::data> gpu_s,
		data& d
	) -> async::task<>;

	[[= gse::system_run<>{}]]
	auto run(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		shared_view<asset::data> assets_s,
		data& d,
		channel_read<sprite_command, text_command> commands_in
	) -> async::task<>;

	[[= gse::system_frame{}]]
	auto frame(
		context& ctx,
		shared_view<gpu::context::data> gpu_s,
		data& d,
		channel_write<gpu::render_pass_request> pass_out,
		shared_view<scene_snapshot::data> snapshot_s
	) -> async::task<>;
}
