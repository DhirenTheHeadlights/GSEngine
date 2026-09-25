import std;

import gse;
import gse.system_manifest;
import installer;

auto main(int argc, char** argv) -> int {
	const auto args = gse::parse_args<installer::arguments>(argc, argv);
	if (args.uninstall) {
		return installer::report(installer::uninstall(installer::own_executable().parent_path().parent_path()));
	}
	if (args.payload.empty()) {
		return installer::report(std::unexpected(std::string("run the GSEngine setup exe, not the installer inside it")));
	}

	auto pack = gse::sdk::read_pack(args.payload);
	if (!pack) {
		return installer::report(std::unexpected(pack.error()));
	}
	installer::set_payload(std::move(*pack));

	gse::start(
		[](gse::engine& e) -> void {
			gse::system_manifest<^^installer::boot::data, ^^installer::boot::run>{}.register_with(e);
		},
		{
			.title = "GSEngine SDK Setup",
			.dark_background = true,
			.video_encode = false,
			.simulate_world = false,
			.custom_chrome = true,
			.launcher_size = { static_cast<int>(installer::window_width), 340 },
			.scale_ui_with_resolution = false,
			.load_settings = false,
			.persist_settings = false,
			.author_baked_assets = false,
		}
	);
	return 0;
}
