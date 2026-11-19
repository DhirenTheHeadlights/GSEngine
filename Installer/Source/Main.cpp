import std;

import gse;
import gse.gpu;
import gse.system_manifest;
import installer;

auto main(int argc, char** argv) -> int {
	const auto args = gse::parse_args<installer::arguments>(argc, argv).config;
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
	const std::string title = pack->table.stamp.product + " Setup";
	installer::set_payload(std::move(*pack));

	gse::start(
		[](gse::engine& e) -> void {
			gse::system_manifest<^^installer::boot::data, ^^installer::boot::run>{}.register_with(e);
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
				gse::settings::assign<^^gse::window::data::launcher_size>({ static_cast<int>(installer::window_width), 340 }),
			},
		}
	);
	return 0;
}
