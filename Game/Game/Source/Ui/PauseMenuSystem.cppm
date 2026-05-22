export module gs:pause_menu_system;

import std;
import gse;

import :crosshair_system;
import :main_menu_screen;

export namespace gs {
	struct pause_menu_system {
		static auto run(
			gse::run_context& ctx,
			const gse::input::system::data& input_d,
			const gse::gui::system::data& gui_d,
			const gse::world_system::data& world_d,
			const gse::network::data& net_d,
			const gse::save::registry& save_reg,
			const crosshair_system::data& crosshair_d
		) -> gse::async::task<>;
	};
}

auto gs::pause_menu_system::run(
	gse::run_context& ctx,
	const gse::input::system::data& input_d,
	const gse::gui::system::data& gui_d,
	const gse::world_system::data& world_d,
	const gse::network::data& net_d,
	const gse::save::registry& save_reg,
	const crosshair_system::data& crosshair_d
) -> gse::async::task<> {
	const auto push_main_menu = [&] {
		ctx.channels.push<gse::gui::push_screen_request>({
			.factory = [&world_d, &net_d, &save_reg, &crosshair_d, channels = ctx.channels] {
				return std::make_unique<main_menu_screen>(world_d, net_d, save_reg, crosshair_d, channels);
			},
		});
	};

	bool manual_cursor = false;
	bool initial_push_done = false;
	bool last_scene_active = false;

	while (true) {
		const auto& input = gse::input::system::current_state(input_d);
		const auto* top = gui_d.menu_stack.top();
		const bool blocks = top != nullptr && !top->dismissable();
		const bool scene_active = world_d.active_scene.has_value();

		if (!blocks && top == nullptr) {
			if (!initial_push_done && !scene_active) {
				push_main_menu();
				initial_push_done = true;
			}
			else if (last_scene_active && !scene_active) {
				push_main_menu();
			}
		}

		last_scene_active = scene_active;

		if (input.key_pressed(gse::key::escape)) {
			if (blocks) {
			}
			else if (top != nullptr) {
				ctx.channels.push<gse::gui::pop_screen_request>({});
			}
			else if (scene_active) {
				push_main_menu();
			}
		}

		const bool cursor_toggle_pressed =
			input.key_pressed(gse::key::n) || input.mouse_button_pressed(gse::mouse_button::button_3);
		if (cursor_toggle_pressed && top == nullptr && scene_active) {
			manual_cursor = !manual_cursor;
			ctx.channels.push<gse::gui::set_manual_cursor_request>({
				.show = manual_cursor
			});
		}
		else if (top != nullptr && manual_cursor) {
			manual_cursor = false;
			ctx.channels.push<gse::gui::set_manual_cursor_request>({
				.show = false
			});
		}

		co_await ctx.next_tick();
	}
}
