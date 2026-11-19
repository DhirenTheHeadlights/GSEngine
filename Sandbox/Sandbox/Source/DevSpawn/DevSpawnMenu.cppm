export module sandbox:dev_spawn_menu;

import gse;

import :dev_spawn_system;

export namespace sandbox::dev_spawn_menu {
	struct [[= gse::system_state<"DevSpawnMenu">{}]]
	data {
	};

	[[= gse::system_run<>{}]]
	auto run(
		gse::context& ctx,
		data& d,
		gse::channel_write<gse::gui::menu_content, spawn_stress_request, spawn_joints_request, spawn_pyramid_request> ui_out,
		gse::shared_view<gse::gui::data> gui_d
	) -> gse::async::task<>;
}

auto sandbox::dev_spawn_menu::run(gse::context& ctx, data& d, const gse::channel_write<gse::gui::menu_content, spawn_stress_request, spawn_joints_request, spawn_pyramid_request> ui_out, const gse::shared_view<gse::gui::data> gui_d) -> gse::async::task<> {
	if (!gui_d.show_dev_overlays) {
		return {};
	}

	ui_out.push<gse::gui::menu_content>({
		.menu = "Dev Spawn",
		.translucent = true,
		.build = [](gse::gui::builder& ui, const gse::channel_write<spawn_stress_request, spawn_joints_request, spawn_pyramid_request> spawn_out) {
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
				spawn_out.push<spawn_stress_request>({});
			}
			if (ui.draw<gse::gui::button>({
					.text = "Joint Test (F6)",
					.key = "##spawn_joints",
					.glyph = gse::gui::symbol::plus(),
				})) {
				spawn_out.push<spawn_joints_request>({});
			}
			if (ui.draw<gse::gui::button>({
					.text = "Pyramid (F9)",
					.key = "##spawn_pyramid",
					.glyph = gse::gui::symbol::plus(),
				})) {
				spawn_out.push<spawn_pyramid_request>({});
			}
		},
	});

	return {};
}
