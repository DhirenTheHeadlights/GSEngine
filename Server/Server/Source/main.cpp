import game;
import gse;
import gse.server;

auto main() -> int {
	gse::set_imgui_enabled(false);
	gse::debug::set_imgui_save_file_path(game::config::resource_path / "imgui_state.ini");
	gse::initialize(gse::server::initialize, gse::server::exit);
	gse::run(gse::server::update, {});
}