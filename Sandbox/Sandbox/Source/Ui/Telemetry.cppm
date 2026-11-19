export module sandbox:telemetry;

import std;
import gse;

import :crosshair_system;

export namespace sandbox::telemetry {
	struct [[= gse::system_state<"Telemetry">{}]]
	data {
		std::string scratch_text;
		gse::gui::text_input_state scratch_text_state;
		float scratch_slider = 0.f;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::gui::menu_content> ui_out,
		gse::shared_view<gse::gui::data> gui_d,
		gse::shared_view<gse::window::data> window_d,
		gse::shared_view<crosshair::data> crosshair_d,
		std::optional<gse::shared_view<gse::renderer::capture::data>> capture_d
	) -> gse::async::task<>;
}

auto sandbox::telemetry::run(gse::context& ctx, data& d, const gse::channel_write<gse::gui::menu_content> ui_out, const gse::shared_view<gse::gui::data> gui_d, const gse::shared_view<gse::window::data> window_d, const gse::shared_view<crosshair::data> crosshair_d, const std::optional<gse::shared_view<gse::renderer::capture::data>> capture_d) -> gse::async::task<> {
	if (!gui_d.show_dev_overlays) {
		return {};
	}

	ui_out.push<gse::gui::menu_content>({
		.menu = "Telemetry",
		.translucent = true,
		.build = [](gse::gui::builder& ui, data& panel_d, const gse::shared_view<gse::window::data> window_s, const gse::shared_view<crosshair::data> crosshair_s, const std::optional<gse::shared_view<gse::renderer::capture::data>> capture_s) {
			const bool recording = capture_s && (*capture_s).recording && (*capture_s).recording->active.load();
			const auto viewport = gse::vec2f(gse::window::viewport(window_s.primary.handle));

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
				.val = static_cast<int>(crosshair_s.show),
			});

			ui.draw<gse::gui::section>({
				.title = "Scratch",
				.subtitle = "Widget playground",
			});
			ui.draw<gse::gui::text_input>({
				.name = "Input",
				.buffer = panel_d.scratch_text,
				.state = panel_d.scratch_text_state,
			});
			ui.draw<gse::gui::slider<float>>({
				.name = "Slider",
				.value = panel_d.scratch_slider,
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

	return {};
}
