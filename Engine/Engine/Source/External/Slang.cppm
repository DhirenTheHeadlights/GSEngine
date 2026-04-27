module;

#include <cstdio>
#include <slang.h>
#include <slang-com-ptr.h>

export module gse.slang;

export namespace slang {
	using ::slang::IBlob;
	using ::slang::IGlobalSession;
	using ::slang::ISession;
	using ::slang::IComponentType;
	using ::slang::IEntryPoint;
	using ::slang::IModule;
	using ::slang::SessionDesc;
	using ::slang::TargetDesc;
	using ::slang::PreprocessorMacroDesc;
	using ::slang::CompilerOptionEntry;
	using ::slang::CompilerOptionName;
	using ::slang::CompilerOptionValue;
	using ::slang::CompilerOptionValueKind;
	using ::slang::ProgramLayout;
	using ::slang::TypeLayoutReflection;
	using ::slang::TypeReflection;
	using ::slang::VariableLayoutReflection;
	using ::slang::VariableReflection;
	using ::slang::EntryPointLayout;
	using ::slang::FunctionReflection;
	using ::slang::Modifier;
	using ::slang::UserAttribute;
	using ::slang::ParameterCategory;
	using ::slang::BindingType;
	using ::slang::createGlobalSession;
}

export namespace Slang {
	using ::Slang::ComPtr;
	using ::Slang::Result;
}

export {
	using ::SlangResult;
	using ::SlangInt;
	using ::SlangUInt;
	using ::SlangInt32;
	using ::SlangUInt32;
	using ::SlangSizeT;
	using ::SlangBindingType;
	using ::SlangBindingTypeIntegral;
	using ::SlangStage;
	using ::SlangResourceShape;
	using ::SlangResourceAccess;
	using ::SlangCompileTarget;
	using ::SlangMatrixLayoutMode;
	using ::SlangParameterCategory;
	using ::ISlangBlob;
	using ::ISlangUnknown;

	inline auto slang_succeeded(SlangResult r) -> bool {
		return SLANG_SUCCEEDED(r);
	}

	inline auto slang_failed(SlangResult r) -> bool {
		return SLANG_FAILED(r);
	}

	inline constexpr SlangBindingTypeIntegral slang_binding_type_base_mask = SLANG_BINDING_TYPE_BASE_MASK;
	inline constexpr SlangBindingTypeIntegral slang_binding_type_mutable_flag = SLANG_BINDING_TYPE_MUTABLE_FLAG;
	inline constexpr SlangBindingType slang_binding_type_combined_texture_sampler = SLANG_BINDING_TYPE_COMBINED_TEXTURE_SAMPLER;
	inline constexpr SlangBindingType slang_binding_type_constant_buffer = SLANG_BINDING_TYPE_CONSTANT_BUFFER;
	inline constexpr SlangBindingType slang_binding_type_input_render_target = SLANG_BINDING_TYPE_INPUT_RENDER_TARGET;
	inline constexpr SlangBindingType slang_binding_type_parameter_block = SLANG_BINDING_TYPE_PARAMETER_BLOCK;
	inline constexpr SlangBindingType slang_binding_type_raw_buffer = SLANG_BINDING_TYPE_RAW_BUFFER;
	inline constexpr SlangBindingType slang_binding_type_ray_tracing_acceleration_structure = SLANG_BINDING_TYPE_RAY_TRACING_ACCELERATION_STRUCTURE;
	inline constexpr SlangBindingType slang_binding_type_sampler = SLANG_BINDING_TYPE_SAMPLER;
	inline constexpr SlangBindingType slang_binding_type_texture = SLANG_BINDING_TYPE_TEXTURE;
	inline constexpr SlangBindingType slang_binding_type_typed_buffer = SLANG_BINDING_TYPE_TYPED_BUFFER;

	inline constexpr SlangResourceShape slang_resource_base_shape_mask = SLANG_RESOURCE_BASE_SHAPE_MASK;
	inline constexpr SlangResourceShape slang_texture_buffer = SLANG_TEXTURE_BUFFER;
	inline constexpr SlangResourceAccess slang_resource_access_read = SLANG_RESOURCE_ACCESS_READ;

	inline constexpr SlangCompileTarget slang_spirv = SLANG_SPIRV;
	inline constexpr SlangMatrixLayoutMode slang_matrix_layout_column_major = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR;

	inline constexpr SlangStage slang_stage_vertex = SLANG_STAGE_VERTEX;
	inline constexpr SlangStage slang_stage_fragment = SLANG_STAGE_FRAGMENT;
	inline constexpr SlangStage slang_stage_compute = SLANG_STAGE_COMPUTE;
	inline constexpr SlangStage slang_stage_mesh = SLANG_STAGE_MESH;
	inline constexpr SlangStage slang_stage_amplification = SLANG_STAGE_AMPLIFICATION;
}
