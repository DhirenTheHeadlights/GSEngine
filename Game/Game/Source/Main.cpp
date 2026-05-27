import std;

import gse;
import gs;

auto main() -> int {
	gse::start([](gse::engine& e) -> void {
		e.add_system<gse::network::system_for<gs::networked_components>>();
		e.add_system<gs::crosshair_system>();
		e.add_system<gs::client_system>();
		e.add_system<gs::client_ui_system>();
		e.add_system<gs::pause_menu_system>();
		e.add_system<gs::dev_spawn_system>();
		e.add_system<gse::gui::popout_system>();
		gs::world_loader_setup(e);
	});
}
