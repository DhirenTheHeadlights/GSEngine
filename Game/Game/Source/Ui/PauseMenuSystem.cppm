export module gs:pause_menu_system;

import std;
import gse;

import :main_menu_screen;

export namespace gs {
	struct pause_menu_system {
		struct data {
			bool manual_cursor = false;
			bool initial_push_done = false;
			bool last_scene_active = false;
		};

		static auto run(
			gse::run_context& ctx,
			data& d,
			const gse::input::system::data& input_d,
			const gse::gui::system::data& gui_d,
			const gse::world_system::data& world_d,
			const gse::network::data& net_d,
			const gse::save::registry& save_reg
		) -> gse::async::task<>;
	};
}

auto gs::pause_menu_system::run(gse::run_context& ctx, data& d, const gse::input::system::data& input_d, const gse::gui::system::data& gui_d, const gse::world_system::data& world_d, const gse::network::data& net_d, const gse::save::registry& save_reg) -> gse::async::task<> {
	const auto push_main_menu = [&] {
		ctx.channels.push<gse::gui::push_screen_request>({
			.factory = [&world_d, &net_d, &save_reg, channels = ctx.channels] {
				return std::make_unique<main_menu_screen>(world_d, net_d, save_reg, channels);
			},
		});
	};

	const auto& input = gse::input::system::current_state(input_d);
	const auto* top = gui_d.menu_stack.top();
	const bool blocks = top != nullptr && !top->dismissable();
	const bool scene_active = world_d.active_scene.has_value();

	if (!blocks && top == nullptr) {
		if (!d.initial_push_done && !scene_active) {
			push_main_menu();
			d.initial_push_done = true;
		}
		else if (d.last_scene_active && !scene_active) {
			push_main_menu();
		}
	}

	d.last_scene_active = scene_active;

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
		d.manual_cursor = !d.manual_cursor;
		ctx.channels.push<gse::gui::set_manual_cursor_request>({
			.show = d.manual_cursor
		});
	}
	else if (top != nullptr && d.manual_cursor) {
		d.manual_cursor = false;
		ctx.channels.push<gse::gui::set_manual_cursor_request>({
			.show = false
		});
	}

	co_return;
}
