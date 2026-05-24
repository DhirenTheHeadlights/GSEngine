export module gs:client_ui;

import std;
import gse;

import :crosshair_system;
import :dev_spawn_system;

export namespace gs {
	struct client_ui_system {
		struct data {
			std::string buff;
			gse::gui::text_input_state buff_state;
			float slider_f = 0.f;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::gui::system::data& gui_d,
			const gse::window::data& window_d,
			const crosshair_system::data& crosshair_d
		) -> gse::async::task<>;
	};
}

namespace gs {
	auto push_crosshair(
		gse::run_context& ctx,
		const gse::gui::system::data& gui_d,
		const gse::window::data& window_d,
		const crosshair_system::data& crosshair_d
	) -> void;
}

auto gs::push_crosshair(gse::run_context& ctx, const gse::gui::system::data& gui_d, const gse::window::data& window_d, const crosshair_system::data& crosshair_d) -> void {
	if (!crosshair_d.show) {
		return;
	}
	if (!gui_d.blank_texture.valid()) {
		return;
	}

	const auto viewport = gse::vec2f(gse::window::viewport(window_d));
	const gse::vec2f center = { viewport.x() * 0.5f, viewport.y() * 0.5f };
	const gse::vec4f color = { crosshair_d.color_r, crosshair_d.color_g, crosshair_d.color_b, crosshair_d.opacity };
	const gse::vec4f outline_color = { 0.f, 0.f, 0.f, crosshair_d.outline_opacity };

	const float arm = crosshair_d.arm_length;
	const float thickness = crosshair_d.arm_thickness;
	const float gap = crosshair_d.gap;
	const float outline = crosshair_d.outline_thickness;

	const auto push_arm = [&](const gse::gui::ui_rect& r) {
		if (outline > 0.f) {
			ctx.channels.push<gse::renderer::sprite_command>({
				.rect = r.inset({ -outline, -outline }),
				.color = outline_color,
				.texture = gui_d.blank_texture,
				.layer = gse::render_layer::overlay,
			});
		}
		ctx.channels.push<gse::renderer::sprite_command>({
			.rect = r,
			.color = color,
			.texture = gui_d.blank_texture,
			.layer = gse::render_layer::overlay,
		});
	};

	if (arm > 0.f) {
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - gap - arm, center.y() + thickness * 0.5f },
				{ arm, thickness }
			)
		);
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() + gap, center.y() + thickness * 0.5f },
				{ arm, thickness }
			)
		);
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - thickness * 0.5f, center.y() + gap + arm },
				{ thickness, arm }
			)
		);
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - thickness * 0.5f, center.y() - gap },
				{ thickness, arm }
			)
		);
	}

	if (crosshair_d.show_dot) {
		const float dot = crosshair_d.dot_size;
		push_arm(
			gse::gui::ui_rect::from_position_size(
				{ center.x() - dot * 0.5f, center.y() + dot * 0.5f },
				{ dot, dot }
			)
		);
	}
}

auto gs::client_ui_system::run(gse::run_context& ctx, data& d, const gse::gui::system::data& gui_d, const gse::window::data& window_d, const crosshair_system::data& crosshair_d) -> gse::async::task<> {
	while (true) {
		if (gui_d.menu_stack.empty()) {
			push_crosshair(ctx, gui_d, window_d, crosshair_d);
		}

		if (gui_d.show_dev_overlays) {
			ctx.channels.push<gse::gui::menu_content>({
				.menu = "Test",
				.build = [&](gse::gui::builder& ui) {
					ui.draw<gse::gui::value<float>>({
						.name = "FPS",
						.val = static_cast<float>(gse::system_clock::fps()),
					});
					ui.draw<gse::gui::value<int>>({
						.name = "Test Value",
						.val = 42,
					});
					ui.draw<gse::gui::text>({
						.content = std::format(
							"Test Quantity: {:.2f}",
							gse::meters(5.0f)
						),
					});
					ui.draw<gse::gui::text_input>({
						.name = "Input Test",
						.buffer = d.buff,
						.state = d.buff_state,
					});
					ui.draw<gse::gui::slider<float>>({
						.name = "Slider Test",
						.value = d.slider_f,
						.min = 0.f,
						.max = 10.f,
					});
				},
			});

			ctx.channels.push<gse::gui::menu_content>({
				.menu = "Profiler",
				.build = [](gse::gui::builder& ui) {
					ui.draw<gse::gui::profiler>();
				},
			});

			ctx.channels.push<gse::gui::menu_content>({
				.menu = "Dev Spawn",
				.build = [&](gse::gui::builder& ui) {
					if (ui.draw<gse::gui::button>({
							.text = "Physics Stress Test (F5)"
						})) {
						ctx.channels.push<gs::spawn_stress_request>({});
					}
					if (ui.draw<gse::gui::button>({
							.text = "Joint Test (F6)"
						})) {
						ctx.channels.push<gs::spawn_joints_request>({});
					}
				},
			});
		}

		co_await ctx.next_tick();
	}
}
