export module sandbox:hud;

import std;
import gse;

import :crosshair_system;

export namespace sandbox::hud {
	struct [[= gse::system_state<"HUD">{}]]
	data {
		[[= gse::actions::bind<"Reset Scene", gse::key::f2>{}]]
		gse::actions::handle reset_scene;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::gui::hud_content, gse::reset_active_scene_request> ui_out,
		gse::shared_view<gse::actions::data> actions_d,
		gse::shared_view<gse::gui::data> gui_d,
		gse::shared_view<gse::window::data> window_d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<crosshair::data> crosshair_d,
		std::optional<gse::shared_view<gse::renderer::capture::data>> capture_d
	) -> gse::async::task<>;
}

namespace sandbox::hud {
	constexpr gse::gui::dp hud_margin{ 24.f };
	constexpr gse::gui::dp recording_dot_size{ 28.f };
	constexpr gse::gui::dp_vec fps_chip_size{ { 96.f, 28.f } };

	[[nodiscard]] auto hud_resettable(
		gse::shared_view<gse::world_system::data> world_s
	) -> bool;

	auto draw_recording_indicator(
		const gse::gui::draw_context& ctx,
		const gse::rect_t<gse::vec2f>& area
	) -> void;

	auto draw_fps_counter(
		gse::gui::builder& ui,
		const gse::rect_t<gse::vec2f>& area
	) -> void;
}

auto sandbox::hud::draw_recording_indicator(const gse::gui::draw_context& ctx, const gse::rect_t<gse::vec2f>& area) -> void {
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

auto sandbox::hud::draw_fps_counter(gse::gui::builder& ui, const gse::rect_t<gse::vec2f>& area) -> void {
	namespace lo = gse::gui::layout;

	const auto& sty = ui.ctx.style;
	const auto chip = lo::align_in(area, fps_chip_size.px(sty), lo::halign::start, lo::valign::top);

	gse::gui::draw::fill_surface(ui.ctx, chip, sty.overlay.tooltip, {});

	const auto readout = std::format("{} FPS", gse::system_clock::fps());

	ui.draw<gse::gui::text>({
		.content = readout,
		.rect = chip.inset(gse::vec2f(sty.padding * 0.5f)),
		.style = {
			.color = sty.overlay.text_color,
			.align = lo::halign::center,
		},
	});
}

auto sandbox::hud::hud_resettable(const gse::shared_view<gse::world_system::data> world_s) -> bool {
	return world_s.active_scene.has_value() && !world_s.networked;
}

auto sandbox::hud::run(gse::context& ctx, data& d, const gse::channel_write<gse::gui::hud_content, gse::reset_active_scene_request> ui_out, const gse::shared_view<gse::actions::data> actions_d, const gse::shared_view<gse::gui::data> gui_d, const gse::shared_view<gse::window::data> window_d, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<crosshair::data> crosshair_d, const std::optional<gse::shared_view<gse::renderer::capture::data>> capture_d) -> gse::async::task<> {
	const bool resettable = hud_resettable(world_d);
	const bool recording = capture_d && (*capture_d).recording && (*capture_d).recording->active.load();

	if (resettable && gse::actions::pressed(d.reset_scene, gse::actions::current_state(actions_d), actions_d)) {
		ui_out.push<gse::reset_active_scene_request>({});
	}

	if (gui_d.primary.menu_stack.empty()) {
		ui_out.push<gse::gui::hud_content>({
			.name = "sandbox.hud",
			.build = [](gse::gui::builder& ui, const gse::shared_view<gse::world_system::data> world_s, const gse::shared_view<crosshair::data> crosshair_s, const gse::channel_write<gse::reset_active_scene_request> reset_out) {
				if (crosshair_s.show) {
					crosshair::draw(ui.ctx, ui.ctx.current_menu->rect, crosshair_s);
				}

				if (!hud_resettable(world_s)) {
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
					reset_out.push<gse::reset_active_scene_request>({});
				}
			},
		});
	}

	ui_out.push<gse::gui::hud_content>({
		.name = "sandbox.fps",
		.build = [](gse::gui::builder& ui) {
			draw_fps_counter(ui, ui.ctx.current_menu->rect.inset(gse::vec2f(hud_margin.px(ui.ctx.style))));
		},
	});

	if (recording && window_d.primary.shown) {
		ui_out.push<gse::gui::hud_content>({
			.name = "sandbox.recording",
			.build = [](gse::gui::builder& ui) {
				const auto& sty = ui.ctx.style;
				const auto area = ui.ctx.current_menu->rect.inset(gse::vec2f(hud_margin.px(sty)));

				const float drop = fps_chip_size.px(sty).y() + sty.padding;

				draw_recording_indicator(ui.ctx, gse::rect_t<gse::vec2f>::from_position_size(
					{ area.left(), area.top() - drop },
					{ area.width(), area.height() - drop }
				));
			},
		});
	}

	return {};
}
