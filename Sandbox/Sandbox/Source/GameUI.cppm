export module sandbox:client_ui;

import std;
import gse;

import :crosshair_system;
import :dev_spawn_system;

export namespace sandbox::client_ui {
	struct [[= gse::system_state<"ClientUI">{}]] data {
		std::string buff;
		gse::gui::text_input_state buff_state;
		float slider_f = 0.f;

		[[= gse::actions::bind<"Reset Scene", gse::key::f2>{}]]
		gse::actions::handle reset_scene;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::gui::menu_content, gse::gui::hud_content, gse::reset_active_scene_request, spawn_stress_request, spawn_joints_request, spawn_pyramid_request> ui_out,
		gse::shared_view<gse::actions::data> actions_d,
		gse::shared_view<gse::gui::data> gui_d,
		gse::shared_view<gse::window::data> window_d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<crosshair::data> crosshair_d,
		std::optional<gse::shared_view<gse::renderer::capture::data>> capture_d
	) -> gse::async::task<>;
}

namespace sandbox::client_ui {
	constexpr gse::gui::dp hud_margin{ 24.f };
	constexpr gse::gui::dp recording_dot_size{ 28.f };

	auto draw_recording_indicator(
		const gse::gui::draw_context& ctx,
		const gse::rect_t<gse::vec2f>& area
	) -> void;
}

auto sandbox::client_ui::draw_recording_indicator(const gse::gui::draw_context& ctx, const gse::rect_t<gse::vec2f>& area) -> void {
	namespace lo = gse::gui::layout;

	const auto& sty = ctx.style;
	const float extent = recording_dot_size.px(sty);
	const float ring = sty.separator_thickness * 2.f;

	const auto dot = lo::align_in(area, { extent, extent }, lo::halign::start, lo::valign::top);
	const auto pulse = 0.55f + 0.45f * gse::sin(gse::radians_per_second(4.f) * gse::system_clock::now());

	ctx.queue_sprite({
		.rect = dot.inset({ -ring, -ring }),
		.color = { 1.f, 1.f, 1.f, 0.9f },
		.texture = ctx.blank_texture,
		.corner_radius = (extent + ring * 2.f) * 0.5f,
	});

	ctx.queue_sprite({
		.rect = dot,
		.color = { 0.95f, 0.15f, 0.15f, pulse },
		.texture = ctx.blank_texture,
		.corner_radius = extent * 0.5f,
	});
}

auto sandbox::client_ui::run(gse::context& ctx, data& d, const gse::channel_write<gse::gui::menu_content, gse::gui::hud_content, gse::reset_active_scene_request, spawn_stress_request, spawn_joints_request, spawn_pyramid_request> ui_out, const gse::shared_view<gse::actions::data> actions_d, const gse::shared_view<gse::gui::data> gui_d, const gse::shared_view<gse::window::data> window_d, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<crosshair::data> crosshair_d, const std::optional<gse::shared_view<gse::renderer::capture::data>> capture_d) -> gse::async::task<> {
	const bool resettable = world_d.active_scene.has_value() && !world_d.networked;
	const bool recording = capture_d && (*capture_d).recording && (*capture_d).recording->active.load();

	if (resettable && gse::actions::pressed(d.reset_scene, gse::actions::current_state(actions_d), actions_d)) {
		ui_out.push<gse::reset_active_scene_request>({});
	}

	if (gui_d.primary.menu_stack.empty()) {
		ui_out.push<gse::gui::hud_content>({
			.name = "sandbox.hud",
			.build = [crosshair_d, resettable, ui_out](gse::gui::builder& ui) {
				if (crosshair_d.show) {
					crosshair::draw(ui.ctx, ui.ctx.current_menu->rect, crosshair_d);
				}

				if (!resettable) {
					return;
				}

				namespace lo = gse::gui::layout;

				const auto& sty = ui.ctx.style;
				const auto area = ui.ctx.current_menu->rect.inset(gse::vec2f(hud_margin.px(sty)));

				if (ui.draw<gse::gui::button>({
						.text = "Reset (F2)",
						.rect = lo::align_in(area, { sty.button_min_width, sty.button_height }, lo::halign::end, lo::valign::top),
						.key = "##hud_reset",
						.glyph = gse::gui::symbol::restart(),
					})) {
					ui_out.push<gse::reset_active_scene_request>({});
				}
			},
		});
	}

	if (recording && window_d.primary.shown) {
		ui_out.push<gse::gui::hud_content>({
			.name = "sandbox.recording",
			.build = [](gse::gui::builder& ui) {
				draw_recording_indicator(ui.ctx, ui.ctx.current_menu->rect.inset(gse::vec2f(hud_margin.px(ui.ctx.style))));
			},
		});
	}

	if (gui_d.show_dev_overlays) {
		const auto viewport = gse::vec2f(gse::window::viewport(window_d.primary.handle));
		const bool crosshair_visible = crosshair_d.show;

		ui_out.push<gse::gui::menu_content>({
			.menu = "Telemetry",
			.translucent = true,
			.build = [&d, viewport, recording, crosshair_visible](gse::gui::builder& ui) {
				ui.draw<gse::gui::section>({
					.title = "Frame",
					.subtitle = "Live client readout",
				});
				ui.draw<gse::gui::value<float>>({
					.name = "Frames per second",
					.val = static_cast<float>(gse::system_clock::fps()),
				});
				ui.draw<gse::gui::value<int>>({
					.name = "Viewport width",
					.val = static_cast<int>(viewport.x()),
				});
				ui.draw<gse::gui::value<int>>({
					.name = "Viewport height",
					.val = static_cast<int>(viewport.y()),
				});

				ui.draw<gse::gui::section>({
					.title = "Capture",
					.subtitle = recording ? "Recording in progress" : "Idle",
				});
				ui.draw<gse::gui::value<int>>({
					.name = "Crosshair visible",
					.val = static_cast<int>(crosshair_visible),
				});

				ui.draw<gse::gui::section>({
					.title = "Scratch",
					.subtitle = "Widget playground",
				});
				ui.draw<gse::gui::text_input>({
					.name = "Input",
					.buffer = d.buff,
					.state = d.buff_state,
				});
				ui.draw<gse::gui::slider<float>>({
					.name = "Slider",
					.value = d.slider_f,
					.min = 0.f,
					.max = 10.f,
				});
			},
		});

		ui_out.push<gse::gui::menu_content>({
			.menu = "Profiler",
			.translucent = true,
			.build = [](gse::gui::builder& ui) {
				ui.draw<gse::gui::profiler>();
			},
		});

		ui_out.push<gse::gui::menu_content>({
			.menu = "Dev Spawn",
			.translucent = true,
			.build = [&](gse::gui::builder& ui) {
				ui.draw<gse::gui::section>({
					.title = "Spawners",
					.subtitle = "Function keys work outside the menu too",
				});
				if (ui.draw<gse::gui::button>({
						.text = "Physics Stress Test (F5)",
						.key = "##spawn_stress",
						.glyph = gse::gui::symbol::plus(),
						.role = gse::gui::button_role::accent,
					})) {
					ui_out.push<spawn_stress_request>({});
				}
				if (ui.draw<gse::gui::button>({
						.text = "Joint Test (F6)",
						.key = "##spawn_joints",
						.glyph = gse::gui::symbol::plus(),
					})) {
					ui_out.push<spawn_joints_request>({});
				}
				if (ui.draw<gse::gui::button>({
						.text = "Pyramid (F9)",
						.key = "##spawn_pyramid",
						.glyph = gse::gui::symbol::plus(),
					})) {
					ui_out.push<spawn_pyramid_request>({});
				}
			},
		});
	}

	return {};
}
