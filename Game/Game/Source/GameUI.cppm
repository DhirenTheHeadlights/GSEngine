export module gs:client_ui;

import std;
import gse;

export namespace gs {
	struct client_ui_system {
		struct data {
			bool show_cross_hair = false;
			std::string buff;
			float slider_f = 0.f;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::input::system::data& input_d
		) -> gse::async::task<>;
	};
}

auto gs::client_ui_system::run(gse::run_context& ctx, data& d, const gse::input::system::data& input_d) -> gse::async::task<> {
	while (true) {
		const auto& input = gse::input::system::current_state(input_d);

		if (input.key_pressed(gse::key::escape)) {
			gse::shutdown();
		}

		if (input.key_pressed(gse::key::n) || input.mouse_button_pressed(gse::mouse_button::button_3)) {
			d.show_cross_hair = !d.show_cross_hair;
		}

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
					.content = std::format("Test Quantity: {:.2f} m", gse::meters(5.0f).as<gse::meters>()),
				});
				ui.draw<gse::gui::input>({
					.name = "Input Test",
					.buffer = d.buff,
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

		ctx.channels.push<gse::ui_focus_request>({
			.focus = d.show_cross_hair,
		});

		co_await ctx.next_tick();
	}
}
