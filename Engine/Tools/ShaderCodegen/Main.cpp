import std;

import gse.containers;
import gse.gpu;
import gse.shader;

template <typename T>
auto emit_enum_only(
	std::ostream& out
) -> void {
	if constexpr (gse::shaders::is_shader_enum<T>) {
		out << gse::shaders::emit_slang_enum<T>() << '\n';
	}
}

template <typename T>
auto emit_struct_only(
	std::ostream& out
) -> void {
	if constexpr (gse::shaders::is_shader_struct<T>) {
		out << gse::shaders::emit_slang_struct<T>() << '\n';
	}
}

template <typename T>
auto emit_binding(
	std::ostream& out
) -> void {
	out << gse::shaders::emit_slang_binding<T>();
}

template <typename... Ts>
auto emit_pack(
	std::ostream& out,
	gse::type_pack<Ts...>
) -> void {
	(emit_enum_only<Ts>(out), ...);
	(emit_struct_only<Ts>(out), ...);
}

template <typename... Ts>
auto emit_binding_pack(
	std::ostream& out,
	gse::type_pack<Ts...>
) -> void {
	(emit_binding<Ts>(out), ...);
}

auto main(const int argc, char** argv) -> int {
	if (argc < 2) {
		std::cerr << "usage: ShaderCodegen <output_dir>\n";
		return 1;
	}

	const std::filesystem::path out_dir = argv[1];
	std::filesystem::create_directories(out_dir);

	{
		std::ofstream out(out_dir / "forward.slang");
		emit_pack(out, gse::shaders::forward::shader_types{});
	}

	{
		std::ofstream out(out_dir / "post_process_layout.slang");
		emit_binding_pack(out, gse::shaders::post_process::shader_binding_types{});
	}

	return 0;
}
