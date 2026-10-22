module gse.gpu_record:shader_compile;

import std;

import gse.gpu_backend;

import :pipeline_builder;

namespace gse::gpu {
	struct shader_param_decl {
		std::string name;
		std::string slang_type;
		std::string semantic;
		bool is_function_param = false;
	};

	struct shader_compile_inputs {
		std::vector<shader_param_decl> params;
		std::uint32_t threads_x = 1;
		std::uint32_t threads_y = 1;
		std::uint32_t threads_z = 1;
		std::string body_path;
		std::string inline_source;
		std::string runtime_constants;
		std::vector<std::string> helper_paths;
		std::vector<std::string> call_names;
		std::uint32_t push_constant_size = 0;
		std::string (
			*emit_push_constant_struct
		)() = nullptr;
		std::string (
			*emit_types
		)() = nullptr;
		std::string (
			*emit_bindings
		)() = nullptr;
		std::string (
			*emit_spec_decls
		)() = nullptr;
	};

	struct parsed_body {
		std::vector<std::string> imports;
		std::string body;
	};

	struct graphics_stage_compile_result {
		stage_flag flag = stage_flag::vertex;
		graphics_stage_kind kind = graphics_stage_kind::vertex;
		std::string entry_point;
		std::vector<std::uint32_t> spirv;
	};

	struct compiled_graphics_program {
		std::vector<graphics_stage_compile_result> stages;
	};

	[[nodiscard]]
	auto compile_compute_spirv(
		const shader_compile_inputs& inputs,
		std::string_view wrapper_source
	) -> std::vector<std::uint32_t>;

	[[nodiscard]]
	auto compile_graphics_program(
		const graphics_entry_pod& pod,
		std::string_view wrapper_source
	) -> compiled_graphics_program;
}
