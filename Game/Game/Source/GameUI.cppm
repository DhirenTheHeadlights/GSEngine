export module gs:client_ui;

import std;
import gse;

export namespace gs {
	struct client_ui_state {
		bool show_cross_hair = false;
		std::string buff;
		float slider_f = 0.f;
	};

	struct client_ui_system {
		static auto update(
			gse::update_context& ctx,
			client_ui_state& s
		) -> gse::async::task<>;
	};
}

auto gs::client_ui_system::update(gse::update_context& ctx, client_ui_state& s) -> gse::async::task<> {
	if (gse::keyboard::pressed(gse::key::escape)) {
		gse::shutdown();
	}

	if (gse::keyboard::pressed(gse::key::n) || gse::mouse::pressed(gse::mouse_button::button_3)) {
		s.show_cross_hair = !s.show_cross_hair;
	}

	gse::gui::panel("Test", [&](gse::gui::builder& ui) {
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
			.buffer = s.buff,
		});
		ui.draw<gse::gui::slider<float>>({
			.name = "Slider Test",
			.value = s.slider_f,
			.min = 0.f,
			.max = 10.f,
		});
	});

	gse::gui::panel("Profiler", [](gse::gui::builder& ui) {
		ui.draw<gse::gui::profiler>();
	});

	if (const auto* pds = ctx.try_state_of<gse::renderer::physics_debug::state>()) {
		if (pds->enabled) {
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
			] = pds->latest_stats;

			gse::gui::panel("Physics Debug", [&](gse::gui::builder& ui) {
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
			});
		}
	}

	gse::set_ui_focus(s.show_cross_hair);

	co_return;
}
