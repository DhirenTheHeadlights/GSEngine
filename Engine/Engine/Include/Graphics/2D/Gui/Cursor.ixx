export module gse.graphics:cursor;

import std;

import :rendering_context;
import :sprite_renderer;
import :texture;

import gse.platform;
import gse.physics.math;

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
        omni_move
    };

    auto set_style(style new_style) -> void;
    auto render(const renderer::context& context, renderer::sprite& sprite_renderer) -> void;
}

namespace gse::cursor {
    auto current_style = style::arrow;

    struct line_params {
        segment_t<unitless::vec2> line;
        float thickness;
        unitless::vec4 color;
        resource::handle<texture> texture;
    };

    auto draw_line(
        renderer::sprite& sprite_renderer,
        const line_params& params
    ) -> void;

    struct arrow_head_params {
        renderer::sprite& sprite_renderer;
        unitless::vec2 tip_position;
        unitless::vec2 direction;
        resource::handle<texture> texture;
    };

    auto draw_arrow_head(
        const arrow_head_params& params
    ) -> void;
}

auto gse::cursor::set_style(const style new_style) -> void {
    current_style = new_style;
}

auto gse::cursor::render(const renderer::context& context, renderer::sprite& sprite_renderer) -> void {
    const auto mouse_pos = mouse::position();
    const auto blank_texture = context.get<texture>(find("blank"));
    constexpr auto color = unitless::vec4{ 1.f, 1.f, 1.f, 1.f };
    constexpr float thickness = 2.f;
    constexpr float length = 22.f;
    constexpr float half_len = length / 2.f;

    switch (current_style) {
	    case style::gameplay:
	    case style::arrow: {
	        draw_line(sprite_renderer, {
	            .line = {
	                { mouse_pos.x() - half_len, mouse_pos.y()},
	                { mouse_pos.x() + half_len, mouse_pos.y()}
	            },
	            .thickness = thickness,
	            .color = color,
	            .texture = blank_texture
	            });
	        draw_line(sprite_renderer, {
	            .line = {
	                { mouse_pos.x(), mouse_pos.y() - half_len},
	                { mouse_pos.x(), mouse_pos.y() + half_len}
	            },
	            .thickness = thickness,
	            .color = color,
	            .texture = blank_texture
	            });
	        break;
	    }

	    case style::resize_e: { 
	        draw_arrow_head({ 
	            .sprite_renderer = sprite_renderer,
        		.tip_position = mouse_pos, 
	            .direction = { 1.f, 0.f },
        		.texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_w: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = { -1.f, 0.f },
	            .texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_n: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = { 0.f, 1.f },
	            .texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_s: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = { 0.f, -1.f },
	            .texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_ne: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = -normalize(unitless::vec2{ 1.f, 1.f }),
	            .texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_sw: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = -normalize(unitless::vec2{ -1.f, -1.f }),
	            .texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_nw: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = -normalize(unitless::vec2{ -1.f, 1.f }),
	            .texture = blank_texture
	        });
	        break;
	    }
	    case style::resize_se: {
	        draw_arrow_head({
	            .sprite_renderer = sprite_renderer,
	            .tip_position = mouse_pos,
	            .direction = -normalize(unitless::vec2{ 1.f, -1.f }),
	            .texture = blank_texture
	        });
	        break;
	    }
		case style::omni_move: {
			constexpr float offset = 10.f;
			draw_arrow_head({
				.sprite_renderer = sprite_renderer,
				.tip_position = mouse_pos + unitless::vec2{ 0.f, offset },
				.direction = { 0.f, 1.f },
				.texture = blank_texture
				});
			draw_arrow_head({
				.sprite_renderer = sprite_renderer,
				.tip_position = mouse_pos + unitless::vec2{ 0.f, -offset },
				.direction = { 0.f, -1.f },
				.texture = blank_texture
			});
			draw_arrow_head({
				.sprite_renderer = sprite_renderer,
				.tip_position = mouse_pos + unitless::vec2{ offset, 0.f },
				.direction = { 1.f, 0.f },
				.texture = blank_texture
			});
			draw_arrow_head({
				.sprite_renderer = sprite_renderer,
				.tip_position = mouse_pos + unitless::vec2{ -offset, 0.f },
				.direction = { -1.f, 0.f },
				.texture = blank_texture
			});
	    }
    default: break;
    }
}

auto gse::cursor::draw_line(renderer::sprite& sprite_renderer, const line_params& params) -> void {
    const float len = params.line.length();
    const auto mid = params.line.midpoint();

    const auto size = unitless::vec2{ len, params.thickness };
    const auto half_size = size / 2.f;

    sprite_renderer.queue({
        .rect = rect_t<unitless::vec2>({
            .min = mid - half_size,
            .max = mid + half_size
        }),
        .color = params.color,
        .texture = params.texture,
        .rotation = params.line.angle(),
    });
}

auto gse::cursor::draw_arrow_head(const arrow_head_params& params) -> void {
    constexpr float head_length = 8.f;
    constexpr angle head_half_angle = degrees(30.f);
    constexpr unitless::vec4 white = { 1.f, 1.f, 1.f, 1.f };
    constexpr float thickness = 2.f;

    const auto base_dir = normalize(params.direction);

    const auto dir1 = rotate(base_dir, head_half_angle);
    const auto dir2 = rotate(base_dir, -head_half_angle);

    draw_line(params.sprite_renderer, {
        .line = { params.tip_position, params.tip_position + dir1 * head_length },
        .thickness = thickness,
        .color = white,
        .texture = params.texture
    });

    draw_line(params.sprite_renderer, {
        .line = { params.tip_position, params.tip_position + dir2 * head_length },
        .thickness = thickness,
        .color = white,
        .texture = params.texture
    });
}