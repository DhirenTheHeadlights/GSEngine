import std;

import gse;
import gse.gpu;
import gse.system_manifest;
import gse.win32;
import gse.win32.environment;
import launcher;

auto main(int argc, char** argv) -> int {
	const auto args = gse::parse_args<launcher::arguments>(argc, argv).config;
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
			.simulate_world = false,
			.load_settings = false,
			.persist_settings = false,
			.author_baked_assets = false,
			.pins = {
				gse::settings::assign<^^gse::gpu::context::data::dark_background>(true),
				gse::settings::assign<^^gse::gpu::context::data::device_settings, ^^gse::gpu::device_settings::video_encode>(false),
				gse::settings::assign<^^gse::gui::data::reserve_top_bar>(true),
				gse::settings::assign<^^gse::gui::data::scale_with_resolution>(false),
				gse::settings::assign<^^gse::window::data::native_frame>(true),
				gse::settings::assign<^^gse::window::data::mouse_visible>(true),
				gse::settings::assign<^^gse::window::data::launcher_size>({ static_cast<int>(launcher::window_width), 220 }),
			},
		}
	);
	return 0;
}
