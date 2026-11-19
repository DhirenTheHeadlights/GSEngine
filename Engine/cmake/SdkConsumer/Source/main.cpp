import std;
import gse;
import gse.config;

auto main() -> int {
	std::println("sdk ok: {}", gse::meters(6.f));
	std::println("engine root: {}", gse::config::root_dir().generic_display_string());
	std::println("engine source: {}", gse::config::source_dir().generic_display_string());
	std::println("baked: {}", gse::config::baked_resource_path().generic_display_string());
	return 0;
}
