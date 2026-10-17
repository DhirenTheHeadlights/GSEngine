import std;

import gse;
import gse.system_manifest;
import gse.win32;
import gse.win32.environment;
import launcher;

auto main(int argc, char** argv) -> int {
	const auto args = gse::parse_args<launcher::arguments>(argc, argv);
	auto local = launcher::own_install();
	if (!local) {
		gse::win32::show_error_box(L"GSE Launcher", gse::win32::widen(local.error()).c_str());
		return 1;
	}
	const std::string title = local->stamp.product + " Launcher";
	launcher::set_install(std::move(*local), args);

	gse::start(
		[](gse::engine& e) -> void {
			gse::system_manifest<^^launcher::boot::data, ^^launcher::boot::run>{}.register_with(e);
		},
		{
			.title = title,
			.dark_background = true,
			.video_encode = false,
			.simulate_world = false,
			.custom_chrome = true,
			.launcher_size = { static_cast<int>(launcher::window_width), 220 },
			.scale_ui_with_resolution = false,
			.load_settings = false,
			.persist_settings = false,
			.author_baked_assets = false,
		}
	);
	return 0;
}
