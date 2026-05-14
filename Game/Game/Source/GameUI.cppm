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
			const gse::input::system::data& input_d,
			const gse::renderer::physics_debug::system::data& pd_d
		) -> gse::async::task<>;
	};
}

auto gs::client_ui_system::run(gse::run_context& ctx, data& d, const gse::input::system::data& input_d, const gse::renderer::physics_debug::system::data& pd_d) -> gse::async::task<> {
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

		if (pd_d.enabled) {
			const auto& [
				body_count,
				sleeping_count,
				contact_count,
				motor_count,
				colliding_pairs,
				solve_time,
				max_linear_speed,
				max_angular_speed,
				max_penetration,
				gpu_solver_active
			] = pd_d.latest_stats;

				ctx.channels.push<gse::gui::menu_content>({
					.menu = "Physics Debug",
					.build = [&](gse::gui::builder& ui) {
					ui.draw<gse::gui::value<std::uint32_t>>({
						.name = "Bodies",
						.val = body_count,
					});
					ui.draw<gse::gui::value<std::uint32_t>>({
						.name = "Sleeping",
						.val = sleeping_count,
					});
					ui.draw<gse::gui::value<std::uint32_t>>({
						.name = "Colliding Pairs",
						.val = colliding_pairs,
					});
					ui.draw<gse::gui::value<float>>({
						.name = "Max Penetration",
						.val = max_penetration.as<gse::meters>(),
					});
					ui.draw<gse::gui::value<float>>({
						.name = "Max Linear Speed",
						.val = max_linear_speed.as<gse::meters_per_second>(),
					});
					ui.draw<gse::gui::value<float>>({
						.name = "Max Angular Speed",
						.val = max_angular_speed.as<gse::radians_per_second>(),
					});
					if (gpu_solver_active) {
						ui.draw<gse::gui::value<std::uint32_t>>({
							.name = "GPU Contacts",
							.val = contact_count,
						});
						ui.draw<gse::gui::value<std::uint32_t>>({
							.name = "GPU Motors",
							.val = motor_count,
						});
						ui.draw<gse::gui::value<float>>({
							.name = "GPU Solve Time (ms)",
							.val = solve_time.as<gse::milliseconds>(),
						});
					}
					},
				});
		}

		ctx.channels.push<gse::ui_focus_request>({
			.focus = d.show_cross_hair,
		});

		co_await ctx.next_tick();
	}
}
