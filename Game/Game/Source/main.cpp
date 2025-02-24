import gse;
import game;

import gse.graphics.vulkan.renderer3d;

auto main() -> int {
	gse::set_imgui_enabled(true);
	gse::debug::set_imgui_save_file_path(game::config::resource_path / "imgui_state.ini");
	gse::initialize(game::initialize, game::close);
	gse::run(game::update, game::render);
}
