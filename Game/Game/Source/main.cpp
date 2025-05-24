import gse;
import game;

void vector_speed_test() {
	//make 100 different vectors for speed test
	gse::unitless::vec4 a{ {1.f, 2.f, 3.f, 4.f} };
	gse::unitless::vec4 b{ {1.f, 2.f, 3.f, 4.f} };
	gse::unitless::vec4 c{ {1.f, 2.f, 3.f, 4.f} };
	for (int i = 0; i < 1000000; i++) {
		gse::dot(a, b);
		gse::dot(b, c);
		gse::dot(a, c);
	}
	//FOR USE IN MAIN LOOP
	//std::cout << "Basic SIMD Support: \nSSE = " << gse::internal::sse_supported << "\n SSE2 = " << gse::internal::sse2_supported << "\nAVX = " << gse::internal::avx_supported << "\nAVX2 = " << gse::internal::avx2_supported << "\n\n";

	//{
	//	gse::scoped_timer main_timer("With SIMD");
	//	vector_speed_test();
	//}

	//gse::internal::sse_supported = false;
	//gse::internal::sse2_supported = false;
	//gse::internal::avx_supported = false;
	//gse::internal::avx2_supported = false;

	//{
	//	gse::scoped_timer main_timer("Without SIMD");
	//	vector_speed_test();
	//}
}

auto main() -> int {
	gse::set_imgui_enabled(true);
	gse::debug::set_imgui_save_file_path(game::config::resource_path / "imgui_state.ini");
	gse::initialize(game::initialize, game::close);
	gse::run(game::update, game::render);
}
	
	

	
