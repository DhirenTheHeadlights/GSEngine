export module gse.graphics:cursor;

import std;

import gse.platform;
import gse.physics.math;

import :rendering_context;
import :ui_renderer;
import :texture;

export namespace gse::cursor {
	enum class style {
		arrow,
		gameplay,
		resize_n,
		resize_ne,
		resize_e,
		resize_se,
		resize_s,
		resize_sw,
		resize_w,
		resize_nw,
		resize_ew,
		omni_move
	};

	auto set_style(
		style new_style
	) -> void;

	auto render_to(
		const renderer::context& context,
		std::vector<renderer::sprite_command>& commands, unitless::vec2 mouse_pos
	) -> void;
}

namespace gse::cursor {
	auto current_style = style::arrow;

	struct line_params {
		segment_t<unitless::vec2> line;
		float thickness;
		unitless::vec4 color;
		resource::handle<texture> texture;
	};

	auto draw_line(std::vector<renderer::sprite_command>& commands, const line_params& params) -> void;

	struct arrow_head_params {
		std::vector<renderer::sprite_command>& commands;
		unitless::vec2 tip_position;
		unitless::vec2 direction;
		resource::handle<texture> texture;
	};

	auto draw_arrow_head(const arrow_head_params& params) -> void;
}

auto gse::cursor::set_style(const style new_style) -> void {
	current_style = new_style;
}

auto gse::cursor::render_to(const renderer::context& context, std::vector<renderer::sprite_command>& commands, const unitless::vec2 mouse_pos) -> void {
	const resource::handle<texture> blank_texture = context.get<texture>(find("blank"));
	constexpr unitless::vec4 color = { 1.f, 1.f, 1.f, 1.f };
	constexpr float length = 22.f;
	constexpr float half_len = length / 2.f;

	switch (current_style) {
	case style::gameplay:
	case style::arrow: {
		constexpr float thickness = 2.f;
		draw_line(commands, {
			.line = {
				{ mouse_pos.x() - half_len, mouse_pos.y() },
				{ mouse_pos.x() + half_len, mouse_pos.y() }
			},
			.thickness = thickness,
			.color = color,
			.texture = blank_texture
		});
		draw_line(commands, {
			.line = {
				{ mouse_pos.x(), mouse_pos.y() - half_len },
				{ mouse_pos.x(), mouse_pos.y() + half_len }
			},
			.thickness = thickness,
			.color = color,
			.texture = blank_texture
		});
		break;
	}
	case style::resize_e: {
		draw_arrow_head({ .commands = commands, .tip_position = mouse_pos, .direction = { 1.f, 0.f }, .texture = blank_texture });
		break;
	}
	case style::resize_w: {
		draw_arrow_head({ .commands = commands, .tip_position = mouse_pos, .direction = { -1.f, 0.f }, .texture = blank_texture });
		break;
	}
	case style::resize_ew: { 
		constexpr float offset = 4.f;
		draw_arrow_head({
			.commands = commands,
			.tip_position = mouse_pos + unitless::vec2{ -offset, 0.f },
			.direction = { -1.f, 0.f },
			.texture = blank_texture
		});
		draw_arrow_head({
			.commands = commands,
			.tip_position = mouse_pos + unitless::vec2{ offset, 0.f },
			.direction = { 1.f, 0.f },
			.texture = blank_texture
		});
		break;
	}
	case style::omni_move: {
		constexpr float offset = 10.f;
		draw_arrow_head({ .commands = commands, .tip_position = mouse_pos + unitless::vec2{ 0.f, offset }, .direction = { 0.f, 1.f }, .texture = blank_texture });
		draw_arrow_head({ .commands = commands, .tip_position = mouse_pos + unitless::vec2{ 0.f, -offset }, .direction = { 0.f, -1.f }, .texture = blank_texture });
		draw_arrow_head({ .commands = commands, .tip_position = mouse_pos + unitless::vec2{ offset, 0.f }, .direction = { 1.f, 0.f }, .texture = blank_texture });
		draw_arrow_head({ .commands = commands, .tip_position = mouse_pos + unitless::vec2{ -offset, 0.f }, .direction = { -1.f, 0.f }, .texture = blank_texture });
		break;
	}
	default:
		break;
	}
}

auto gse::cursor::draw_line(std::vector<renderer::sprite_command>& commands, const line_params& params) -> void {
	const float len = params.line.length();
	const unitless::vec2 mid = params.line.midpoint();
	const unitless::vec2 size = { len, params.thickness };
	const unitless::vec2 half_size = size / 2.f;

	commands.push_back({
		.rect = rect_t<unitless::vec2>({
			.min = mid - half_size,
			.max = mid + half_size
		}),
		.color = params.color,
		.texture = params.texture,
		.rotation = params.line.angle(),
		.layer = render_layer::cursor
	});
}

auto gse::cursor::draw_arrow_head(const arrow_head_params& params) -> void {
	constexpr float head_length = 8.f;
	constexpr angle head_half_angle = degrees(30.f);
	constexpr unitless::vec4 white = { 1.f, 1.f, 1.f, 1.f };
	constexpr float thickness = 2.f;

	const unitless::vec2 base_dir = normalize(params.direction);
	const unitless::vec2 dir1 = rotate(base_dir, head_half_angle);
	const unitless::vec2 dir2 = rotate(base_dir, -head_half_angle);

	draw_line(params.commands, {
		.line = { params.tip_position, params.tip_position + dir1 * head_length },
		.thickness = thickness,
		.color = white,
		.texture = params.texture
	});

	draw_line(params.commands, {
		.line = { params.tip_position, params.tip_position + dir2 * head_length },
		.thickness = thickness,
		.color = white,
		.texture = params.texture
	});
}