export module sandbox:pause_menu_system;

import std;
import gse;

import :main_menu_screen;

export namespace sandbox::pause_menu {
	struct [[= gse::system_state<"PauseMenu">{}]] data {
		bool manual_cursor = false;
		bool initial_push_done = false;
		bool last_scene_active = false;
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::activate_scene_request, gse::deactivate_active_scene_request, gse::gui::push_screen_request, gse::gui::pop_screen_request, gse::gui::set_manual_cursor_request, gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::settings::change_request, gse::gui::popout_toggle> ui_out,
		gse::shared_view<gse::input::data> input_d,
		gse::shared_view<gse::gui::data> gui_d,
		gse::shared_view<gse::world_system::data> world_d,
		gse::shared_view<gse::network::data> net_d,
		const gse::network::config& net_cfg,
		const gse::save::registry& save_reg
	) -> gse::async::task<>;
}

auto sandbox::pause_menu::run(gse::context& ctx, data& d, const gse::channel_write<gse::activate_scene_request, gse::deactivate_active_scene_request, gse::gui::push_screen_request, gse::gui::pop_screen_request, gse::gui::set_manual_cursor_request, gse::network::connect_request, gse::network::refresh_servers_request, gse::network::refresh_server_info_request, gse::network::ping_request, gse::settings::change_request, gse::gui::popout_toggle> ui_out, const gse::shared_view<gse::input::data> input_d, const gse::shared_view<gse::gui::data> gui_d, const gse::shared_view<gse::world_system::data> world_d, const gse::shared_view<gse::network::data> net_d, const gse::network::config& net_cfg, const gse::save::registry& save_reg) -> gse::async::task<> {
	const auto push_main_menu = [&] {
		ui_out.push<gse::gui::push_screen_request>({
			.factory = [world_d, net_d, &save_reg, channels = ui_out] {
				return std::make_unique<main_menu_screen>(world_d, net_d, save_reg, channels);
			},
		});
	};

	const auto& input = gse::input::current_state(input_d);
	const auto* top = gui_d.primary.menu_stack.top();
	const bool blocks = top != nullptr && !top->dismissable();
	const bool scene_active = world_d.active_scene.has_value();
	const bool server_owns_scene = !net_cfg.connect.empty();

	if (!blocks && top == nullptr && !server_owns_scene) {
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
			ui_out.push<gse::gui::pop_screen_request>({});
		}
		else {
			push_main_menu();
		}
	}

	const bool cursor_toggle_pressed =
		input.key_pressed(gse::key::n) || input.mouse_button_pressed(gse::mouse_button::button_3);
	if (cursor_toggle_pressed && top == nullptr && scene_active) {
		d.manual_cursor = !d.manual_cursor;
		ui_out.push<gse::gui::set_manual_cursor_request>({
			.show = d.manual_cursor
		});
	}
	else if (top != nullptr && d.manual_cursor) {
		d.manual_cursor = false;
		ui_out.push<gse::gui::set_manual_cursor_request>({
			.show = false
		});
	}

	return {};
}
