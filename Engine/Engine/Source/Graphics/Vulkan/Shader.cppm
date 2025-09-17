module;

#include <cstdio>
#include <slang-com-ptr.h>
#include <slang.h>

#undef assert

export module gse.graphics:shader;

import std;

import :rendering_context;

import gse.assert;
import gse.platform;
import gse.utility;

namespace gse {
	export class shader final : public identifiable, non_copyable {
	public:
		struct info {
			std::string name;
			std::filesystem::path path;
		};

		struct vertex_input {
			std::vector<vk::VertexInputBindingDescription> bindings;
			std::vector<vk::VertexInputAttributeDescription> attributes;
		};

		struct uniform_member {
			std::string name;
			std::string type_name;
			std::uint32_t offset = 0;
			std::uint32_t size = 0;
			std::uint32_t array_size = 0;
		};

		struct uniform_block {
			std::string name;
			std::uint32_t binding = 0;
			std::uint32_t set = 0;
			std::uint32_t size = 0;
			std::unordered_map<std::string, uniform_member> members;
			vk::ShaderStageFlags stage_flags;
		};

		struct binding {
			std::string name;
			vk::DescriptorSetLayoutBinding layout_binding;
			std::optional<uniform_block> member;
		};

		struct set {
			enum class binding_type : std::uint8_t {
				persistent = 0,
				push = 1,
				bind_less = 2,
			};

			binding_type type;
			std::variant<std::monostate, std::shared_ptr<vk::raii::DescriptorSetLayout>, vk::DescriptorSetLayout> layout;
			std::vector<binding> bindings;
		};

		struct layout {
			std::unordered_map<set::binding_type, set> sets;
		};

		enum class descriptor_layout : std::uint8_t {
			deferred_3d = 0,
			forward_3d = 1,
			post_process = 2,
			standard_2d = 3,
			standard_3d = 4,
			custom = 99
		};

		explicit shader(const std::filesystem::path& path);

		static auto compile() -> std::set<std::filesystem::path>;

		auto load(
			const renderer::context& context
		) -> void;

		auto unload() -> void;
		static auto destroy_global_layouts() -> void;

		auto shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
		auto required_bindings() const -> std::vector<std::string>;
		auto uniform_block(const std::string& name) const -> uniform_block;
		auto uniform_blocks() const -> std::vector<class uniform_block>;
		auto binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding>;
		auto layout(set::binding_type type) const -> vk::DescriptorSetLayout;
		auto layouts() const -> std::vector<vk::DescriptorSetLayout>;
		auto push_constant_range(const std::string_view& name) const -> vk::PushConstantRange;
		auto vertex_input_state() const -> vk::PipelineVertexInputStateCreateInfo;

		auto descriptor_writes(
			vk::DescriptorSet set,
			const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
			const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos
		) const -> std::vector<vk::WriteDescriptorSet>;

		template <typename T>
		auto set_uniform(
			std::string_view full_name,
			const T& value,
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;

		auto set_uniform_block(
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& data,
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;

		auto set_ssbo_element(
			std::string_view block_name,   
			std::uint32_t index,        
			std::string_view member_name, 
			std::span<const std::byte> bytes,
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;

		auto set_ssbo_struct(
			std::string_view block_name,
			std::uint32_t index,
			std::span<const std::byte> element_bytes,
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;

		auto push_descriptor(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view name,
			const vk::DescriptorImageInfo& image_info
		) const -> void;

		auto push(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view block_name,
			const void* data,
			std::size_t offset = 0
		) const -> void;

		auto push(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& values
		) const -> void;

		auto descriptor_set(
			const vk::raii::Device& device,
			vk::DescriptorPool pool,
			set::binding_type type,
			std::uint32_t count
		) const -> std::vector<vk::raii::DescriptorSet>;

		auto descriptor_set(
			const vk::raii::Device& device,
			vk::DescriptorPool pool,
			set::binding_type type
		) const -> vk::raii::DescriptorSet;
	private:
		vk::raii::ShaderModule m_vert_module = nullptr;
		vk::raii::ShaderModule m_frag_module = nullptr;

		struct layout m_layout;
		vertex_input m_vertex_input;
		std::vector<struct uniform_block> m_push_constants;
		info m_info;

		static auto layout(const set& set) -> vk::DescriptorSetLayout {
			if (std::holds_alternative<std::shared_ptr<vk::raii::DescriptorSetLayout>>(set.layout)) {
				return **std::get<std::shared_ptr<vk::raii::DescriptorSetLayout>>(set.layout);
			}
			if (std::holds_alternative<vk::DescriptorSetLayout>(set.layout)) {
				return std::get<vk::DescriptorSetLayout>(set.layout);
			}
			assert(false, "Shader set layout is not initialized");
			return {};
		}
	};

	std::unordered_map<std::string, shader::descriptor_layout> shader_layout_types;
	std::unordered_map<shader::descriptor_layout, struct shader::layout> global_layouts;
}

gse::shader::shader(const std::filesystem::path& path) : identifiable(path) {
	m_info = {
		.name = path.stem().string(),	
		.path = path
	};
}

auto gse::shader::compile() -> std::set<std::filesystem::path> {
	const std::unordered_map<std::string, descriptor_layout> string_to_layout_enum{
		{ "standard_3d",	descriptor_layout::standard_3d	},
		{ "deferred_3d",	descriptor_layout::deferred_3d	},
		{ "forward_3d",		descriptor_layout::forward_3d	},
		{ "standard_2d",	descriptor_layout::standard_2d	},
		{ "post_process",	descriptor_layout::post_process	},
	};

	const auto root_path = config::resource_path / "Shaders";
	const auto layout_dir_path = root_path / "Layouts";
	const auto destination_path = config::baked_resource_path / "Shaders";

	auto write_string = [](
		std::ofstream& stream,
		const std::string& str
		) {
			const auto size = static_cast<std::uint32_t>(str.size());
			stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
			stream.write(str.c_str(), size);
		};

	auto write_data = [](
		std::ofstream& stream,
		const auto& value
		) {
			stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
		};

	auto to_vk_vertex_format = [](
		slang::TypeReflection* type
		) -> vk::Format {
			using kind = slang::TypeReflection::Kind;

			auto elem_and_count = [](
				slang::TypeReflection* t
				) -> std::pair<slang::TypeReflection*, int> {
					switch (t->getKind()) {
						case kind::Vector: return { t->getElementType(), static_cast<int>(t->getElementCount()) };
						case kind::Scalar: return { t, 1 };
						default: return { t, 0 };
					}
				};

			auto [elem, count] = elem_and_count(type);

			if (!elem || count <= 0) return vk::Format::eUndefined;

			switch (elem->getScalarType()) {
				case slang::TypeReflection::Float32:
					switch (count) {
						case 1: return vk::Format::eR32Sfloat;
						case 2: return vk::Format::eR32G32Sfloat;
						case 3: return vk::Format::eR32G32B32Sfloat;
						case 4: return vk::Format::eR32G32B32A32Sfloat;
						default: return vk::Format::eUndefined;
					}
				case slang::TypeReflection::Int32:
					switch (count) {
						case 1: return vk::Format::eR32Sint;
						case 2: return vk::Format::eR32G32Sint;
						case 3: return vk::Format::eR32G32B32Sint;
						case 4: return vk::Format::eR32G32B32A32Sint;
						default: return vk::Format::eUndefined;
					}
				case slang::TypeReflection::UInt32:
					switch (count) {
						case 1: return vk::Format::eR32Uint;
						case 2: return vk::Format::eR32G32Uint;
						case 3: return vk::Format::eR32G32B32Uint;
						case 4: return vk::Format::eR32G32B32A32Uint;
						default: return vk::Format::eUndefined;
					}
				default: break;
			}

			assert(false, "Unsupported vertex attribute type/size for Vulkan format");
			return vk::Format::eUndefined;
		};

	auto to_vk_descriptor_type = [](
		slang::TypeLayoutReflection* tl,
		const int range_index
		) -> vk::DescriptorType {
			if (tl == nullptr) {
				return vk::DescriptorType::eStorageBuffer;
			}

			if (const int range_count = tl->getBindingRangeCount(); range_index < 0 || range_index >= range_count) {
				return vk::DescriptorType::eStorageBuffer;
			}

			const slang::BindingType bt = tl->getBindingRangeType(range_index);
			const SlangBindingTypeIntegral bt_bits = static_cast<SlangBindingTypeIntegral>(bt);

			const auto base_bt = static_cast<SlangBindingType>(bt_bits & SLANG_BINDING_TYPE_BASE_MASK);
			const bool is_mutable = (bt_bits & SLANG_BINDING_TYPE_MUTABLE_FLAG) != 0;

			slang::TypeLayoutReflection* leaf_layout = tl->getBindingRangeLeafTypeLayout(range_index);
			slang::TypeReflection* leaf_type = leaf_layout ? leaf_layout->getType() : nullptr;

			const auto access = leaf_type ? leaf_type->getResourceAccess() : SLANG_RESOURCE_ACCESS_READ;
			const auto shape = leaf_type ? static_cast<SlangResourceShape>(leaf_type->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) : static_cast<SlangResourceShape>(0);

			switch (base_bt) {
				case SLANG_BINDING_TYPE_COMBINED_TEXTURE_SAMPLER: {
					return vk::DescriptorType::eCombinedImageSampler;
				}

				case SLANG_BINDING_TYPE_TEXTURE: {
					if (shape == SLANG_TEXTURE_BUFFER) {
						if (is_mutable || access != SLANG_RESOURCE_ACCESS_READ) {
							return vk::DescriptorType::eStorageTexelBuffer;
						}
						return vk::DescriptorType::eUniformTexelBuffer;
					}

					if (is_mutable || access != SLANG_RESOURCE_ACCESS_READ) {
						return vk::DescriptorType::eStorageImage;
					}
					return vk::DescriptorType::eSampledImage;
				}

				case SLANG_BINDING_TYPE_SAMPLER: {
					return vk::DescriptorType::eSampler;
				}

				case SLANG_BINDING_TYPE_TYPED_BUFFER:
				{
					if (is_mutable || access != SLANG_RESOURCE_ACCESS_READ) {
						return vk::DescriptorType::eStorageTexelBuffer;
					}
					return vk::DescriptorType::eUniformTexelBuffer;
				}

				case SLANG_BINDING_TYPE_RAW_BUFFER: {
					return vk::DescriptorType::eStorageBuffer;
				}

				case SLANG_BINDING_TYPE_CONSTANT_BUFFER:
				case SLANG_BINDING_TYPE_PARAMETER_BLOCK: {
					return vk::DescriptorType::eUniformBuffer;
				}

				case SLANG_BINDING_TYPE_INPUT_RENDER_TARGET: {
					return vk::DescriptorType::eSampledImage;
				}

				case SLANG_BINDING_TYPE_RAY_TRACING_ACCELERATION_STRUCTURE: {
					return vk::DescriptorType::eAccelerationStructureKHR;
				}

				default: {
					return vk::DescriptorType::eStorageBuffer;
				}
			}
		};

	auto reflect_members = [](
		slang::TypeLayoutReflection* struct_layout,
		const std::optional<slang::ParameterCategory> preferred_cat = std::nullopt
		) -> std::unordered_map<std::string, uniform_member> {
			using kind = slang::TypeReflection::Kind;

			std::unordered_map<std::string, uniform_member> members;
			if (!struct_layout || struct_layout->getKind() != kind::Struct) {
				return members;
			}

			const slang::ParameterCategory order[] = {
				preferred_cat.value_or(slang::ParameterCategory::Uniform),
				slang::ParameterCategory::ShaderResource,
				slang::ParameterCategory::UnorderedAccess,
				slang::ParameterCategory::Uniform
			};

			slang::ParameterCategory chosen = order[0];
			bool ok = false;

			for (const auto c : order) {
				if (struct_layout->getSize(c) > 0) {
					chosen = c; ok = true;
					break;
				}
			}

			if (!ok) {
				chosen = slang::ParameterCategory::Uniform;
			}

			for (int i = 0; i < struct_layout->getFieldCount(); ++i) {
				auto* m_var = struct_layout->getFieldByIndex(i);
				auto* m_type = m_var ? m_var->getTypeLayout() : nullptr;
				if (!m_type) {
					continue;
				}

				const auto k = m_type->getKind();
				if (k == kind::Resource || k == kind::SamplerState) {
					continue;
				}

				std::uint32_t sz = static_cast<std::uint32_t>(m_type->getSize(chosen));
				if (sz == 0) {
					sz = static_cast<std::uint32_t>(m_type->getSize(slang::ParameterCategory::Uniform));
				}
				if (sz == 0) {
					continue;
				}

				std::uint32_t off = static_cast<std::uint32_t>(m_var->getOffset(chosen));
				if (off == 0 && sz != 0) {
					off = static_cast<std::uint32_t>(m_var->getOffset(slang::ParameterCategory::Uniform));
				}

				members[m_var->getName()] = {
					.name = m_var->getName(),
					.type_name = m_type->getType() ? m_type->getType()->getName() : std::string{},
					.offset = off,
					.size = sz,
					.array_size = (k == kind::Array) ? static_cast<std::uint32_t>(m_type->getElementCount()) : 0u
				};
			}
			return members;
		};

	auto extract_struct_layout = [](
		slang::VariableLayoutReflection* var
		) -> slang::TypeLayoutReflection* {
			if (!var) {
				return nullptr;
			}

			auto* type = var->getTypeLayout();

			if (!type) {
				return nullptr;
			}

			slang::TypeLayoutReflection* struct_layout = nullptr;
			switch (type->getKind()) {
				case slang::TypeReflection::Kind::ConstantBuffer:
				case slang::TypeReflection::Kind::ParameterBlock:
					if (auto* elem_var = type->getElementVarLayout()) struct_layout = elem_var->getTypeLayout();
					break;
				case slang::TypeReflection::Kind::Struct:
					struct_layout = type;
					break;
				default: break;
			}

			return struct_layout && struct_layout->getKind() == slang::TypeReflection::Kind::Struct ? struct_layout : nullptr;
		};

	auto reflect_descriptor_sets = [&](
		slang::ProgramLayout* program_layout
		) -> struct layout {
			struct layout result;
			auto* globals_vl = program_layout->getGlobalParamsVarLayout();

			if (!globals_vl) {
				return result;
			}

			auto* globals_tl = globals_vl->getTypeLayout();
			if (!globals_tl) {
				return result;
			}

			slang::VariableLayoutReflection* container_vl = nullptr;
			slang::TypeLayoutReflection* element_tl = nullptr;

			switch (globals_tl->getKind()) {
				case slang::TypeReflection::Kind::Struct:
					container_vl = globals_vl;
					element_tl = globals_tl;
					break;
				case slang::TypeReflection::Kind::ConstantBuffer:
				case slang::TypeReflection::Kind::ParameterBlock:
					container_vl = globals_tl->getContainerVarLayout();
					if (auto* element_vl = globals_tl->getElementVarLayout()) {
						element_tl = element_vl->getTypeLayout();
					}
					break;
				default: break;
			}

			if (!element_tl || element_tl->getKind() != slang::TypeReflection::Kind::Struct) {
				return result;
			}

			const auto container_space = static_cast<std::uint32_t>(container_vl ? container_vl->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot) : 0);
			const int field_count = element_tl->getFieldCount();

			for (int i = 0; i < field_count; ++i) {
				auto* var = element_tl->getFieldByIndex(i);
				auto* tl = var->getTypeLayout();

				const auto binding_range_count = tl->getBindingRangeCount();

				for (int range_index = 0; range_index < binding_range_count; ++range_index) {
					const auto binding = static_cast<std::uint32_t>(var->getOffset(slang::ParameterCategory::DescriptorTableSlot));
					const auto set_idx = container_space + static_cast<std::uint32_t>(var->getBindingSpace(slang::ParameterCategory::DescriptorTableSlot));

					auto set_type = static_cast<set::binding_type>(set_idx);
					auto& current_set = result.sets[set_type];
					current_set.type = set_type;

					vk::DescriptorSetLayoutBinding layout_binding{
						.binding = binding,
						.descriptorType = to_vk_descriptor_type(tl, range_index),
						.descriptorCount = tl->getKind() == slang::TypeReflection::Kind::Array ? static_cast<std::uint32_t>(tl->getElementCount()) : 1u,
					};

					using kind = slang::TypeReflection::Kind;

					auto try_structured_buffer_block = [&](
						slang::VariableLayoutReflection* variable,
						slang::TypeLayoutReflection* type_layout,
						const std::uint32_t binding_t,
						const std::uint32_t set_index
						) -> std::optional<struct uniform_block> {
							using kind = slang::TypeReflection::Kind;

							const bool is_structured =
								type_layout->getKind() == kind::ShaderStorageBuffer ||
								(type_layout->getKind() == kind::Resource &&
									(type_layout->getType()->getResourceShape() & SLANG_RESOURCE_BASE_SHAPE_MASK) == SLANG_STRUCTURED_BUFFER);

							if (!is_structured) {
								return std::nullopt;
							}

							slang::TypeLayoutReflection* elem_tl = nullptr;

							if (type_layout->getKind() == kind::ShaderStorageBuffer) {
								if (auto* evl = type_layout->getElementVarLayout())
									elem_tl = evl->getTypeLayout();
							}
							else {
								elem_tl = type_layout->getElementTypeLayout();
							}

							if (!elem_tl || elem_tl->getKind() != kind::Struct) {
								return std::nullopt;
							}

							const auto cat = (type_layout->getKind() == kind::ShaderStorageBuffer) ? slang::ParameterCategory::UnorderedAccess : slang::ParameterCategory::ShaderResource;

							std::uint32_t elem_stride = static_cast<std::uint32_t>(elem_tl->getStride(cat));
							if (elem_stride == 0) {
								const std::uint32_t sz = static_cast<std::uint32_t>(elem_tl->getSize(slang::ParameterCategory::Uniform));
								std::uint32_t al = static_cast<std::uint32_t>(elem_tl->getAlignment(slang::ParameterCategory::Uniform));

								if (al == 0) al = 16;
								if (sz) elem_stride = (sz + (al - 1)) & ~(al - 1);
							}

							auto members = reflect_members(elem_tl, cat);

							if (elem_stride == 0 && !members.empty()) {
								uint32_t max_end = 0;
								for (const auto& m : members | std::views::values) {
									max_end = std::max(max_end, m.offset + m.size);
								}
								constexpr std::uint32_t align = 16;
								elem_stride = (max_end + (align - 1)) & ~(align - 1);
							}

							if (elem_stride == 0 && members.empty()) return std::nullopt;

							struct uniform_block block {
								.name = variable->getName(),
									.binding = binding_t,
									.set = set_index,
									.size = elem_stride,
									.members = std::move(members),
							};

							return block;
						};

					std::optional<struct uniform_block> block_member;

					if (tl->getKind() == kind::ConstantBuffer || tl->getKind() == kind::ParameterBlock) {
						if (auto* struct_layout = extract_struct_layout(var)) {
							struct uniform_block block = {
								.name = var->getName(),
								.binding = binding,
								.set = set_idx,
								.size = static_cast<std::uint32_t>(struct_layout->getSize(slang::ParameterCategory::Uniform)),
								.members = reflect_members(struct_layout, slang::ParameterCategory::Uniform)
							};
							if (block.size > 0 && !block.members.empty()) block_member = block;
						}
					}
					else {
						block_member = try_structured_buffer_block(var, tl, binding, set_idx);
					}

					current_set.bindings.emplace_back(var->getName(), layout_binding, block_member);
				}
			}
			return result;
		};

	auto to_vk_stage_flags = [](
		const SlangStage slang_stage
		) -> vk::ShaderStageFlags {
			switch (slang_stage) {
				case SLANG_STAGE_VERTEX:    return vk::ShaderStageFlagBits::eVertex;
				case SLANG_STAGE_FRAGMENT:  return vk::ShaderStageFlagBits::eFragment;
				case SLANG_STAGE_GEOMETRY:  return vk::ShaderStageFlagBits::eGeometry;
				case SLANG_STAGE_COMPUTE:   return vk::ShaderStageFlagBits::eCompute;
				default:					return {};
			}
		};

	auto process_push_constant_variable = [&](
		slang::VariableLayoutReflection* var,
		std::vector<struct uniform_block>& pcs,
		const vk::ShaderStageFlags stage
		) {
			if (!var || var->getCategory() != slang::ParameterCategory::PushConstantBuffer) {
				return;
			}

			auto* struct_layout = extract_struct_layout(var);
			if (!struct_layout) {
				return;
			}

			const std::string block_name = struct_layout->getName() && struct_layout->getName()[0]
				? struct_layout->getName()
				: "push_constants";

			const auto it = std::ranges::find_if(
				pcs, 
				[&](const auto& b) {
					return b.name == block_name;
				});

			if (it != pcs.end()) {
				it->stage_flags |= stage;
			}
			else {
				struct uniform_block b {
					.name = block_name,
					.size = static_cast<std::uint32_t>(struct_layout->getSize(slang::ParameterCategory::Uniform)),
					.members = reflect_members(struct_layout, slang::ParameterCategory::Uniform),
					.stage_flags = stage 
				};
				pcs.push_back(std::move(b));
			}
		};

	Slang::ComPtr<slang::IGlobalSession> global_session;
	{
		const auto r = createGlobalSession(global_session.writeRef());
		assert(SLANG_SUCCEEDED(r) && global_session, "Failed to create Slang global session");
	}

	Slang::ComPtr<slang::ISession> session;
	{
		slang::TargetDesc target{
			.format = SLANG_SPIRV,
			.profile = global_session->findProfile("spirv_1_5")
		};

		std::vector<std::string> sp_storage;
		std::vector<const char*> sp_c_strs;

		sp_storage.push_back(root_path.string());
		for (const auto& e : std::filesystem::recursive_directory_iterator(root_path)) {
			if (e.is_directory()) sp_storage.push_back(e.path().string());
		}

		sp_c_strs.reserve(sp_storage.size());
		for (auto& s : sp_storage) {
			sp_c_strs.push_back(s.c_str());
		}

		slang::SessionDesc sdesc{
			.targets = &target,
			.targetCount = 1,
			.defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
			.searchPaths = sp_c_strs.data(),
			.searchPathCount = static_cast<SlangInt>(sp_c_strs.size()),
		};

		const auto rs = global_session->createSession(sdesc, session.writeRef());
		assert(SLANG_SUCCEEDED(rs) && session, "Failed to create Slang session");
	}

	global_layouts.clear();
	shader_layout_types.clear();
	std::set<std::filesystem::path> compiled_assets;

	if (exists(layout_dir_path)) {
		for (const auto& entry : std::filesystem::directory_iterator(layout_dir_path)) {
			const auto& path = entry.path();
			const std::string filename = path.filename().string();

			if (!entry.is_regular_file()) {
				continue;
			}

			const std::string layout_name = path.stem().string();

			auto it = string_to_layout_enum.find(layout_name);
			assert(it != string_to_layout_enum.end(), "Layout file '" + filename + "' has no matching descriptor_layout enum.");

			const descriptor_layout type = it->second;
			shader_layout_types[layout_name] = type;

			Slang::ComPtr<slang::IBlob> diags;
			Slang::ComPtr mod(session->loadModule(path.string().c_str(), diags.writeRef()));
			if (diags && diags->getBufferSize()) {
				std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
			}
			assert(mod, std::format("Failed to load layout module '{}'", filename));

			Slang::ComPtr<slang::IComponentType> program;
			{
				const auto lr = mod->link(program.writeRef(), diags.writeRef());
				if (diags && diags->getBufferSize()) {
					std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
				}
				assert(SLANG_SUCCEEDED(lr) && program, "Failed to link layout program");
			}

			slang::ProgramLayout* layout = program->getLayout(0, diags.writeRef());

			if (diags && diags->getBufferSize()) {
				std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
			}
			assert(layout, "Failed to get ProgramLayout for layout");

			struct layout reflected_layout = reflect_descriptor_sets(layout);

			vk::ShaderStageFlags used_stages = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;
			if (layout->getEntryPointCount() > 0) {
				used_stages = {};
				for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
					auto* ep = layout->getEntryPointByIndex(epi);
					if (!ep) {
						continue;
					}
					switch (ep->getStage()) {
						case SLANG_STAGE_VERTEX:   used_stages |= vk::ShaderStageFlagBits::eVertex;   break;
						case SLANG_STAGE_FRAGMENT: used_stages |= vk::ShaderStageFlagBits::eFragment; break;
						default: break;
					}
				}
			}
			for (auto& s_data : reflected_layout.sets | std::views::values) {
				for (auto& [n, lb, mem] : s_data.bindings) {
					lb.stageFlags = used_stages;
				}
			}

			global_layouts.emplace(type, std::move(reflected_layout));
		}
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".slang") {
			continue;
		}

		const auto source_path = entry.path();
		const std::string filename = source_path.filename().string();
		if (source_path.parent_path() == layout_dir_path || filename == "common.slang") {
			continue;
		}

		const std::string base_name = source_path.stem().string();
		const auto dest_asset_file = destination_path / (base_name + ".gshader");
		compiled_assets.insert(dest_asset_file);
		if (exists(dest_asset_file) && last_write_time(source_path) <= last_write_time(dest_asset_file)) {
			continue;
		}

		if (auto dst_dir = dest_asset_file.parent_path(); !exists(dst_dir)) {
			create_directories(dst_dir);
		}

		Slang::ComPtr<slang::IBlob> diags;
		Slang::ComPtr<slang::IModule> mod;

		mod = session->loadModule(source_path.string().c_str(), diags.writeRef());
		if (diags && diags->getBufferSize()) {
			std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
		}
		assert(mod, std::format("Failed to load module '{}'", filename));

		Slang::ComPtr<slang::IEntryPoint> vs_ep, fs_ep;
		{
			const int ep_count = mod->getDefinedEntryPointCount();
			for (int i = 0; i < ep_count; ++i) {
				Slang::ComPtr<slang::IEntryPoint> ep;
				mod->getDefinedEntryPoint(i, ep.writeRef());
				if (const char* n = ep->getFunctionReflection()->getName(); !std::strcmp(n, "vs_main")) {
					vs_ep = ep;
				}
				else if (!std::strcmp(n, "fs_main")) {
					fs_ep = ep;
				}
			}
		}
		assert(vs_ep && fs_ep, std::format("Shader '{}' must define vs_main and fs_main", filename));

		Slang::ComPtr<slang::IComponentType> composed;
		{
			slang::IComponentType* parts[] = { vs_ep.get(), fs_ep.get() };
			const auto cr = session->createCompositeComponentType(parts, 2, composed.writeRef(), diags.writeRef());
			if (diags && diags->getBufferSize()) {
				std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
			}
			assert(SLANG_SUCCEEDED(cr) && composed, "Failed to compose shader program");
		}

		Slang::ComPtr<slang::IComponentType> program;
		{
			const auto lr = composed->link(program.writeRef(), diags.writeRef());
			if (diags && diags->getBufferSize()) {
				std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
			}
			assert(SLANG_SUCCEEDED(lr) && program, "Failed to link shader program");
		}

		auto map_layout_kind = [](
			const SlangInt v
			) -> std::optional<descriptor_layout> {
				switch (v) {
					case 0: return descriptor_layout::deferred_3d;
					case 1: return descriptor_layout::forward_3d;
					case 2: return descriptor_layout::post_process;
					case 3: return descriptor_layout::standard_2d;
					case 4: return descriptor_layout::standard_3d;
					default: return std::nullopt;
				}
			};

		auto layout_from_attr = [&](
			slang::IEntryPoint* ep
			) -> std::optional<descriptor_layout> {
				if (!ep) {
					return std::nullopt;
				}

				auto* fn = ep->getFunctionReflection();

				slang::UserAttribute* attr = fn->findUserAttributeByName(session->getGlobalSession(), "Layout");

				if (!attr) {
					return std::nullopt;
				}

				int v = 0;
				if (SLANG_SUCCEEDED(attr->getArgumentValueInt(0, &v))) {
					return map_layout_kind(v);
				}
				return std::nullopt;
			};

		auto v_kind = layout_from_attr(vs_ep.get());
		auto f_kind = layout_from_attr(fs_ep.get());

		if (v_kind && f_kind) {
			assert(*v_kind == *f_kind, std::format("Mismatched [Layout(...)] on vs_main/fs_main in shader: {}", filename));
		}

		auto shader_layout_type = v_kind.value_or(f_kind.value_or(descriptor_layout::custom));

		slang::ProgramLayout* layout = program->getLayout(0, diags.writeRef());

		if (diags && diags->getBufferSize()) {
			std::fprintf(stderr, "%s", static_cast<const char*>(diags->getBufferPointer()));
		}
		assert(layout, "Failed to get ProgramLayout");

		vertex_input reflected_vertex_input;
		std::unordered_map<set::binding_type, set> reflected_sets;
		std::vector<struct uniform_block> reflected_pcs;

		auto unwrap_to_struct = [](
			slang::TypeLayoutReflection* tl
			) -> slang::TypeLayoutReflection* {
				using k = slang::TypeReflection::Kind;
				if (!tl) {
					return nullptr;
				}
				if (tl->getKind() == k::Struct) {
					return tl;
				}
				if (tl->getKind() == k::ConstantBuffer || tl->getKind() == k::ParameterBlock) {
					if (auto* evl = tl->getElementVarLayout()) {
						auto* etl = evl->getTypeLayout();
						return (etl && etl->getKind() == k::Struct) ? etl : nullptr;
					}
				}
				return nullptr;
			};

		auto explicit_location = [&](
			slang::VariableLayoutReflection* vl,
			slang::IGlobalSession* g
			) -> std::optional<std::uint32_t> {
				if (!vl) return {
					std::nullopt
				};

				if (auto* var = vl->getVariable()) {
					if (auto* a = var->findUserAttributeByName(g, "vk::location")) {
						int v = 0;
						if (SLANG_SUCCEEDED(a->getArgumentValueInt(0, &v)) && v >= 0) {
							return static_cast<uint32_t>(v);
						}
					}
					if (auto* a = var->findUserAttributeByName(g, "location")) {
						int v = 0;
						if (SLANG_SUCCEEDED(a->getArgumentValueInt(0, &v)) && v >= 0) {
							return static_cast<uint32_t>(v);
						}
					}
				}

				if (const char* sem = vl->getSemanticName()) {
					if (std::strncmp(sem, "SV_", 3) && std::strncmp(sem, "builtin_", 8)) {
						const char* end = sem + std::strlen(sem);
						const char* p = end;
						while (p > sem && std::isdigit(static_cast<unsigned char>(p[-1]))) {
							--p;
						}

						if (p < end) {
							const int n = std::atoi(p);
							return static_cast<std::uint32_t>(n);
						}
					}
				}
				return std::nullopt;
			};

		for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
			auto* ep = layout->getEntryPointByIndex(epi);
			if (!ep || ep->getStage() != SLANG_STAGE_VERTEX) {
				continue;
			}

			auto* scope_vl = ep->getVarLayout();                 
			auto* scope_tl = scope_vl ? scope_vl->getTypeLayout() : nullptr;
			auto* param_struct = unwrap_to_struct(scope_tl);
			if (!param_struct) {
				continue;
			}

			std::uint32_t next_loc = 0;
			std::unordered_set<std::uint32_t> used_locations;

			std::function<void(slang::VariableLayoutReflection*)> visit_input;
			visit_input = [&](
				slang::VariableLayoutReflection* vl
				) {
					if (!vl) {
						return;
					}
					if (const char* sem = vl->getSemanticName()) {
						if (!std::strncmp(sem, "SV_", 3) || !std::strncmp(sem, "builtin_", 8)) {
							return;
						}
					}

					using k = slang::TypeReflection::Kind;
					auto* tl = vl->getTypeLayout();
					if (!tl) {
						return;
					}

					if (tl->getKind() == k::Struct) {
						const int n = tl->getFieldCount();
						for (int i = 0; i < n; ++i) {
							visit_input(tl->getFieldByIndex(i));
						}
						return;
					}
					if (tl->getKind() == k::Array) {
						if (auto* evl = tl->getElementVarLayout()) {
							visit_input(evl);
						}
						return;
					}
					if (tl->getKind() == k::Resource || tl->getKind() == k::SamplerState ||
						tl->getKind() == k::Matrix || tl->getKind() == k::ConstantBuffer ||
						tl->getKind() == k::ParameterBlock) {
						return;
					}

					auto* ty = tl->getType();
					if (!ty) {
						return;
					}

					const vk::Format fmt = to_vk_vertex_format(ty);
					assert(fmt != vk::Format::eUndefined, "Unsupported vertex attribute type");

					std::uint32_t loc;
					if (const auto el = explicit_location(vl, session->getGlobalSession()); el && !used_locations.contains(*el)) {
						loc = *el;
					}
					else {
						while (used_locations.contains(next_loc)) {
							++next_loc;
						}
						loc = next_loc++;
					}
					used_locations.insert(loc);

					reflected_vertex_input.attributes.push_back(vk::VertexInputAttributeDescription{
						.location = loc,
						.binding = 0,
						.format = fmt,
						.offset = 0
					});
				};

			const int field_count = param_struct->getFieldCount();
			for (int i = 0; i < field_count; ++i) {
				visit_input(param_struct->getFieldByIndex(i));
			}
		}

		std::ranges::sort(
			reflected_vertex_input.attributes, 
			{}, 
			&vk::VertexInputAttributeDescription::location
		);

		if (shader_layout_type == descriptor_layout::custom) {
			auto [sets] = reflect_descriptor_sets(layout);
			vk::ShaderStageFlags used_stages{};

			for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
				auto* ep = layout->getEntryPointByIndex(epi);
				if (!ep) {
					continue;
				}
				switch (ep->getStage()) {
					case SLANG_STAGE_VERTEX:   used_stages |= vk::ShaderStageFlagBits::eVertex;   break;
					case SLANG_STAGE_FRAGMENT: used_stages |= vk::ShaderStageFlagBits::eFragment; break;
					default: break;
				}
			}

			for (auto& s_data : sets | std::views::values) {
				for (auto& [n, lb, mem] : s_data.bindings) lb.stageFlags = used_stages;
			}

			reflected_sets = std::move(sets);
		}

		for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
			if (auto* ep = layout->getEntryPointByIndex(epi)) {
				vk::ShaderStageFlags stage_flag = to_vk_stage_flags(ep->getStage());
				process_push_constant_variable(ep->getVarLayout(), reflected_pcs, stage_flag);
			}
		}

		if (auto* globals_vl = layout->getGlobalParamsVarLayout()) {
			vk::ShaderStageFlags all_entry_point_stages = {};
			for (std::uint32_t epi = 0; epi < layout->getEntryPointCount(); ++epi) {
				if (auto* ep = layout->getEntryPointByIndex(epi)) {
					all_entry_point_stages |= to_vk_stage_flags(ep->getStage());
				}
			}

			if (auto* struct_layout = extract_struct_layout(globals_vl)) {
				for (int i = 0; i < struct_layout->getFieldCount(); ++i) {
					process_push_constant_variable(
						struct_layout->getFieldByIndex(i),
						reflected_pcs,
						all_entry_point_stages 
					);
				}
			}
		}

		std::ofstream out(dest_asset_file, std::ios::binary);
		assert(out.is_open(), std::format("Failed to create gshader file: {}", dest_asset_file.string()));
		write_data(out, shader_layout_type);
		write_data(out, static_cast<std::uint32_t>(reflected_vertex_input.attributes.size()));
		for (const auto& attr : reflected_vertex_input.attributes) write_data(out, attr);
		if (shader_layout_type == descriptor_layout::custom) {
			write_data(out, static_cast<std::uint32_t>(reflected_sets.size()));
			for (const auto& [type, set_data] : reflected_sets) {
				write_data(out, type);
				write_data(out, static_cast<std::uint32_t>(set_data.bindings.size()));
				for (const auto& [name, layout_binding, member] : set_data.bindings) {
					write_string(out, name);
					write_data(out, layout_binding);
					write_data(out, member.has_value());
					if (member) {
						write_string(out, member->name);
						write_data(out, member->binding);
						write_data(out, member->set);
						write_data(out, member->size);
						write_data(out, static_cast<std::uint32_t>(member->members.size()));
						for (const auto& [m_name, m_data] : member->members) {
							write_string(out, m_name);
							write_string(out, m_data.name);
							write_data(out, m_data.offset);
							write_data(out, m_data.size);
							write_data(out, m_data.array_size);
						}
					}
				}
			}
		}
		write_data(out, static_cast<std::uint32_t>(reflected_pcs.size()));
		for (const auto& pc : reflected_pcs) {
			write_string(out, pc.name);
			write_data(out, pc.size);
			write_data(out, pc.stage_flags);
			write_data(out, static_cast<std::uint32_t>(pc.members.size()));
			for (const auto& [m_name, m_data] : pc.members) {
				write_string(out, m_name);
				write_string(out, m_data.name);
				write_data(out, m_data.offset);
				write_data(out, m_data.size);
				write_data(out, m_data.array_size);
			}
		}
		{
			Slang::ComPtr<ISlangBlob> vert_blob, frag_blob, gen_diags;
			const auto rv = program->getEntryPointCode(0, 0, vert_blob.writeRef(), gen_diags.writeRef());
			const auto rf = program->getEntryPointCode(1, 0, frag_blob.writeRef(), gen_diags.writeRef());
			if (gen_diags && gen_diags->getBufferSize()) {
				std::fprintf(stderr, "%s", static_cast<const char*>(gen_diags->getBufferPointer()));
			}
			assert(SLANG_SUCCEEDED(rv) && vert_blob, "Failed to get vertex SPIR-V");
			assert(SLANG_SUCCEEDED(rf) && frag_blob, "Failed to get fragment SPIR-V");
			const std::size_t vert_size = vert_blob->getBufferSize();
			const std::size_t frag_size = frag_blob->getBufferSize();
			write_data(out, vert_size);
			out.write(static_cast<const char*>(vert_blob->getBufferPointer()), vert_size);
			write_data(out, frag_size);
			out.write(static_cast<const char*>(frag_blob->getBufferPointer()), frag_size);
		}

		std::println("Baking shader asset: {} (Layout: {})",
			dest_asset_file.filename().string(),
			shader_layout_type == descriptor_layout::custom ? "Custom" : "Global"
		);
	}

	return compiled_assets;
}

auto gse::shader::load(const renderer::context& context) -> void {
	std::ifstream in(m_info.path, std::ios::binary);
	assert(in.is_open(), std::format("Failed to open gshader asset: {}", m_info.path.string()));

	auto read_string = [](
		std::ifstream& stream,
		std::string& str
		) {
			std::uint32_t size;
			stream.read(reinterpret_cast<char*>(&size), sizeof(size));
			str.resize(size);
			stream.read(str.data(), size);
		};

	auto read_data = [](
		std::ifstream& stream,
		auto& value
		) {
			stream.read(reinterpret_cast<char*>(&value), sizeof(value));
		};

	descriptor_layout shader_layout_type{};
	read_data(in, shader_layout_type);

	std::uint32_t attr_count = 0;
	read_data(in, attr_count);
	m_vertex_input.attributes.resize(attr_count);
	for (auto& attr : m_vertex_input.attributes) read_data(in, attr);

	if (shader_layout_type == descriptor_layout::custom) {
		std::uint32_t num_sets = 0;
		read_data(in, num_sets);
		for (std::uint32_t i = 0; i < num_sets; ++i) {
			set::binding_type type;
			read_data(in, type);
			std::uint32_t num_bindings = 0;
			read_data(in, num_bindings);
			std::vector<struct binding> bindings;
			for (std::uint32_t j = 0; j < num_bindings; ++j) {
				struct binding b;
				read_string(in, b.name);
				read_data(in, b.layout_binding);
				bool has_member = false;
				read_data(in, has_member);
				if (has_member) {
					struct uniform_block member;
					read_string(in, member.name);
					read_data(in, member.binding);
					read_data(in, member.set);
					read_data(in, member.size);
					std::uint32_t num_ubo_members = 0;
					read_data(in, num_ubo_members);
					for (std::uint32_t k = 0; k < num_ubo_members; ++k) {
						std::string key_name;
						uniform_member ubo_member;
						read_string(in, key_name);
						read_string(in, ubo_member.name);
						read_data(in, ubo_member.offset);
						read_data(in, ubo_member.size);
						read_data(in, ubo_member.array_size);
						member.members[key_name] = ubo_member;
					}
					b.member = member;
				}
				bindings.push_back(std::move(b));
			}
			m_layout.sets[type] = { .type = type, .bindings = std::move(bindings) };
		}
	}
	else {
		assert(global_layouts.contains(shader_layout_type), "Global shader layout not found");
		m_layout = global_layouts.at(shader_layout_type);
	}

	std::uint32_t pc_count = 0;
	read_data(in, pc_count);
	m_push_constants.resize(pc_count);
	for (auto& pc : m_push_constants) {
		read_string(in, pc.name);
		read_data(in, pc.size);
		read_data(in, pc.stage_flags);
		std::uint32_t member_count = 0;
		read_data(in, member_count);
		for (std::uint32_t i = 0; i < member_count; ++i) {
			std::string key, name;
			std::uint32_t offset = 0, size = 0, arr = 0;
			read_string(in, key);
			read_string(in, name);
			read_data(in, offset);
			read_data(in, size);
			read_data(in, arr);
			pc.members[key] = {
				.name = name,
				.type_name = {},
				.offset = offset,
				.size = size,
				.array_size = arr
			};
		}
	}

	std::size_t vert_size = 0, frag_size = 0;
	read_data(in, vert_size);
	std::vector<char> vert_code(vert_size);
	in.read(vert_code.data(), vert_size);

	read_data(in, frag_size);
	std::vector<char> frag_code(frag_size);
	in.read(frag_code.data(), frag_size);

	m_vert_module = context.config().device_config().device.createShaderModule({
		.codeSize = vert_code.size(),
		.pCode = reinterpret_cast<const std::uint32_t*>(vert_code.data())
	});
	m_frag_module = context.config().device_config().device.createShaderModule({
		.codeSize = frag_code.size(),
		.pCode = reinterpret_cast<const std::uint32_t*>(frag_code.data())
	});

	if (!m_vertex_input.attributes.empty()) {
		std::ranges::sort(m_vertex_input.attributes, {}, &vk::VertexInputAttributeDescription::location);
		std::uint32_t stride = 0;
		auto format_size = [](
			const vk::Format format
			) -> std::uint32_t {
				switch (format) {
					case vk::Format::eR32Sfloat:
					case vk::Format::eR32Sint:
					case vk::Format::eR32Uint:                 return 4;
					case vk::Format::eR32G32Sfloat:
					case vk::Format::eR32G32Sint:
					case vk::Format::eR32G32Uint:              return 8;
					case vk::Format::eR32G32B32Sfloat:
					case vk::Format::eR32G32B32Sint:
					case vk::Format::eR32G32B32Uint:           return 12;
					case vk::Format::eR32G32B32A32Sfloat:
					case vk::Format::eR32G32B32A32Sint:
					case vk::Format::eR32G32B32A32Uint:        return 16;
					default:
						assert(false, "Unsupported vertex format in size calc");
						return 0;
				}
			};

		for (auto& attr : m_vertex_input.attributes) {
			attr.offset = stride;
			stride += format_size(attr.format);
		}

		m_vertex_input.bindings.push_back({
			.binding = 0,
			.stride = stride,
			.inputRate = vk::VertexInputRate::eVertex
		});
	}

	for (auto& [type, set_data] : m_layout.sets) {
		std::vector<vk::DescriptorSetLayoutBinding> raw_bindings;
		raw_bindings.reserve(set_data.bindings.size());

		for (const auto& b : set_data.bindings) {
			auto lb = b.layout_binding;
			lb.pImmutableSamplers = nullptr;     
			raw_bindings.push_back(lb);
		}

		vk::DescriptorSetLayoutCreateInfo ci{
			.flags = (type == set::binding_type::push) ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags(),
			.bindingCount = static_cast<uint32_t>(raw_bindings.size()),
			.pBindings = raw_bindings.data()
		};

		set_data.layout = std::make_shared<vk::raii::DescriptorSetLayout>(context.config().device_config().device, ci);
	}

	std::uint32_t max_set_index = 0;
	for (const auto& t : m_layout.sets | std::views::keys) {
		max_set_index = std::max(max_set_index, static_cast<uint32_t>(t));
	}

	for (uint32_t i = 0; i <= max_set_index; ++i) {
		if (auto t = static_cast<set::binding_type>(i); !m_layout.sets.contains(t)) {
			vk::DescriptorSetLayoutCreateInfo ci{
				.flags = t == set::binding_type::push ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags(),
				.bindingCount = 0,
				.pBindings = nullptr
			};
			set empty_set{
				.type = t,
				.layout = std::make_shared<vk::raii::DescriptorSetLayout>(context.config().device_config().device, ci)
			};
			m_layout.sets.emplace(t, std::move(empty_set));
		}
	}
}

auto gse::shader::unload() -> void {
	m_vert_module = nullptr;
	m_frag_module = nullptr;
	m_layout = {};
	m_vertex_input = {};
	m_push_constants.clear();
	m_info = {};
}

auto gse::shader::destroy_global_layouts() -> void {
	global_layouts.clear();
	shader_layout_types.clear();
}

auto gse::shader::shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2> {
	return {
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = *m_vert_module,
			.pName = "main"
		},
		vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eFragment,
			.module = *m_frag_module,
			.pName = "main"
		}
	};
}

auto gse::shader::required_bindings() const -> std::vector<std::string> {
	std::vector<std::string> required_bindings;
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [member_name, layout_binding, member] : s.bindings) {
			if (layout_binding.descriptorType == vk::DescriptorType::eStorageBuffer) {
				required_bindings.push_back(member_name);
			}
		}
	}
	return required_bindings;
}

auto gse::shader::uniform_block(const std::string& name) const -> class uniform_block {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& b : s.bindings) {
			if (b.member.has_value() && b.member.value().name == name) {
				return b.member.value();
			}
		}
	}
	assert(
		false,
		std::format("Uniform block '{}' not found in shader", name)
	);
	return {};
}

auto gse::shader::uniform_blocks() const -> std::vector<struct uniform_block> {
	std::vector<struct uniform_block> blocks;
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& b : s.bindings) {
			if (b.member.has_value()) {
				blocks.push_back(b.member.value());
			}
		}
	}
	return blocks;
}

auto gse::shader::binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding> {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [binding_name, layout_binding, member] : s.bindings) {
			if (binding_name == name) {
				return layout_binding;
			}
		}
	}
	return std::nullopt;
}

auto gse::shader::layout(const set::binding_type type) const -> vk::DescriptorSetLayout {
	assert(
		m_layout.sets.contains(type),
		std::format(
			"Shader does not have set of type {}",
			static_cast<int>(type)
		)
	);
	return layout(m_layout.sets.at(type));
}

auto gse::shader::layouts() const -> std::vector<vk::DescriptorSetLayout> {
	std::map<set::binding_type, vk::DescriptorSetLayout> sorted_layouts;
	for (const auto& [type, set] : m_layout.sets) {
		sorted_layouts[type] = layout(set);
	}

	std::vector<vk::DescriptorSetLayout> result;
	for (const auto& layout : sorted_layouts | std::views::values) {
		result.push_back(layout);
	}
	return result;
}

auto gse::shader::push_constant_range(const std::string_view& name) const -> vk::PushConstantRange {
	const auto it = std::ranges::find_if(
		m_push_constants,
		[&](const auto& block) {
			return block.name == name;
		}
	);

	assert(it != m_push_constants.end(), std::format("Push constant block '{}' not found in shader", name));

	return vk::PushConstantRange{
		it->stage_flags,
		0,
		it->size
	};
}

auto gse::shader::vertex_input_state() const -> vk::PipelineVertexInputStateCreateInfo {
	return {
		.flags = {},
		.vertexBindingDescriptionCount = static_cast<std::uint32_t>(m_vertex_input.bindings.size()),
		.pVertexBindingDescriptions = m_vertex_input.bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(m_vertex_input.attributes.size()),
		.pVertexAttributeDescriptions = m_vertex_input.attributes.data()
	};
}

auto gse::shader::descriptor_writes(const vk::DescriptorSet set, const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos, const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos) const -> std::vector<vk::WriteDescriptorSet> {
	std::vector<vk::WriteDescriptorSet> writes;
	std::unordered_set<std::string> used_keys;

	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [binding_name, layout_binding, member] : s.bindings) {
			if (auto it = buffer_infos.find(binding_name); it != buffer_infos.end()) {
				writes.emplace_back(vk::WriteDescriptorSet{
					.pNext = nullptr,
					.dstSet = set,
					.dstBinding = layout_binding.binding,
					.dstArrayElement = 0,
					.descriptorCount = layout_binding.descriptorCount,
					.descriptorType = layout_binding.descriptorType,
					.pImageInfo = nullptr,
					.pBufferInfo = &it->second,
					.pTexelBufferView = nullptr
				});
				used_keys.insert(binding_name);
			}
			else if (auto it2 = image_infos.find(binding_name); it2 != image_infos.end()) {
				writes.emplace_back(vk::WriteDescriptorSet{
					.pNext = nullptr,
					.dstSet = set,
					.dstBinding = layout_binding.binding,
					.dstArrayElement = 0,
					.descriptorCount = layout_binding.descriptorCount,
					.descriptorType = layout_binding.descriptorType,
					.pImageInfo = &it2->second,
					.pBufferInfo = nullptr,
					.pTexelBufferView = nullptr
				});
				used_keys.insert(binding_name);
			}

		}
	}
	const auto total_inputs = buffer_infos.size() + image_infos.size();
	assert(
		used_keys.size() == total_inputs,
		"Some descriptor inputs were not used. Possibly extra or misnamed keys?"
	);
	return writes;
}


template <typename T>
auto gse::shader::set_uniform(std::string_view full_name, const T& value, const vulkan::persistent_allocator::allocation& alloc) const -> void {
	const auto dot_pos = full_name.find('.');

	assert(
		dot_pos != std::string_view::npos,
		std::format(
			"Uniform name '{}' must be in the format 'Block.member'",
			full_name
		)
	);

	const auto block_name = std::string(full_name.substr(0, dot_pos));
	const auto member_name = std::string(full_name.substr(dot_pos + 1));

	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name != block_name) {
				continue;
			}

			assert(
				b.member.has_value(),
				std::format(
					"No uniform block data for '{}'",
					block_name
				)
			);

			const auto& ub = b.member.value();

			const auto mem_it = ub.members.find(member_name);

			assert(
				mem_it != ub.members.end(),
				std::format(
					"Member '{}' not found in block '{}'",
					member_name,
					block_name
				)
			);

			const auto& mem_info = mem_it->second;

			assert(
				sizeof(T) <= mem_info.size,
				std::format(
					"Value size {} exceeds member '{}' size {}",
					sizeof(T),
					member_name,
					mem_info.size
				)
			);

			assert(
				alloc.mapped(),
				std::format(
					"Attempted to set uniform '{}.{}' but memory is not mapped",
					block_name,
					member_name
				)
			);

			std::memcpy(
				static_cast<std::byte*>(alloc.mapped()) + mem_info.offset,
				&value,
				sizeof(T)
			);
			return;
		}
	}

	assert(
		false,
		std::format("Uniform block '{}' not found", block_name)
	);
}

auto gse::shader::set_uniform_block(const std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>& data, const vulkan::persistent_allocator::allocation& alloc) const -> void {
	const struct binding* block_binding = nullptr;
	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name == block_name && b.member.has_value()) {
				block_binding = &b;
				break;
			}
		}
		if (block_binding) {
			break;
		}
	}
	assert(block_binding, std::format("Uniform block '{}' not found", block_name));

	const auto& block = *block_binding->member;
	assert(alloc.mapped(), "Attempted to set uniform block but memory is not mapped");

	for (const auto& [name, bytes] : data) {
		auto member_it = block.members.find(name);

		assert(
			member_it != block.members.end(),
			std::format("Uniform member '{}' not found in block '{}'", name, block_name)
		);

		const auto& member_info = member_it->second;

		assert(
			bytes.size() <= member_info.size,
			std::format("Data size {} > member size {} for '{}.{}'", bytes.size(), member_info.size, block_name, name)
		);

		std::memcpy(
			static_cast<std::byte*>(alloc.mapped()) + member_info.offset,
			bytes.data(),
			bytes.size()
		);
	}
}

auto gse::shader::set_ssbo_element(std::string_view block_name, const std::uint32_t index, std::string_view member_name, const std::span<const std::byte> bytes, const vulkan::persistent_allocator::allocation& alloc) const -> void {
	const struct binding* block_binding = nullptr;
	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name == block_name && b.member.has_value()) {
				block_binding = &b;
				break;
			}
		}
		if (block_binding) break;
	}
	assert(block_binding, std::format("SSBO '{}' not found", block_name));

	const auto& block = *block_binding->member;
	const auto elem_stride = block.size;
	assert(elem_stride > 0, std::format("SSBO '{}' has zero element stride", block_name));

	assert(alloc.mapped(), "Attempted to set SSBO but memory is not mapped");

	const auto mit = block.members.find(std::string(member_name));
	assert(
		mit != block.members.end(),
		std::format("Member '{}' not found in SSBO '{}'", member_name, block_name)
	);

	const auto& m_info = mit->second;
	assert(
		bytes.size() <= m_info.size,
		std::format(
			"Bytes size {} > member '{}' size {} in SSBO '{}'",
			bytes.size(), 
			member_name,
			m_info.size,
			block_name
		)
	);

	const auto base = static_cast<std::byte*>(alloc.mapped());
	std::memcpy(
		base + index * elem_stride + m_info.offset,
		bytes.data(),
		bytes.size()
	);
}

auto gse::shader::set_ssbo_struct(std::string_view block_name, const std::uint32_t index, const std::span<const std::byte> element_bytes, const vulkan::persistent_allocator::allocation& alloc) const -> void {
	const struct binding* block_binding = nullptr;
	for (const auto& set : m_layout.sets | std::views::values) {
		for (const auto& b : set.bindings) {
			if (b.name == block_name && b.member.has_value()) {
				block_binding = &b;
				break;
			}
		}
		if (block_binding) break;
	}
	assert(block_binding, std::format("SSBO '{}' not found", block_name));

	const auto& block = *block_binding->member;
	const auto elem_stride = block.size;

	assert(elem_stride > 0, std::format("SSBO '{}' has zero element stride", block_name));
	assert(alloc.mapped(), "Attempted to set SSBO but memory is not mapped");

	assert(
		element_bytes.size() == elem_stride,
		std::format("Element bytes {} != stride {} for SSBO '{}'", element_bytes.size(), elem_stride, block_name)
	);

	const auto base = static_cast<std::byte*>(alloc.mapped());
	std::memcpy(
		base + index * elem_stride,
		element_bytes.data(),
		element_bytes.size()
	);
}

auto gse::shader::push_descriptor(const vk::CommandBuffer command, const vk::PipelineLayout layout, const std::string_view name, const vk::DescriptorImageInfo& image_info) const -> void {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [member_name, layout_binding, member] : s.bindings) {
			if (member_name == name) {
				assert(
					layout_binding.descriptorType == vk::DescriptorType::eCombinedImageSampler || layout_binding.descriptorType == vk::DescriptorType::eSampledImage,
					std::format("Binding '{}' is not a combined image sampler", name)
				);

				auto info = image_info;

				if (layout_binding.descriptorType == vk::DescriptorType::eSampledImage) {
					info.sampler = nullptr;
				}

				command.pushDescriptorSetKHR(
					vk::PipelineBindPoint::eGraphics,
					layout,
					static_cast<std::uint32_t>(s.type),
					{
						vk::WriteDescriptorSet{
							.pNext = {},
							.dstBinding = layout_binding.binding,
							.dstArrayElement = 0,
							.descriptorCount = 1,
							.descriptorType = layout_binding.descriptorType,
							.pImageInfo = &info,
						}
					}
				);
				return;
			}
		}
	}
	assert(false, std::format("Binding '{}' not found in shader", name));
}

auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, const void* data, const std::size_t offset) const -> void {
	const auto it = std::ranges::find_if(
		m_push_constants, 
		[&](const auto& block) {
			return block.name == block_name;
		});

	assert(it != m_push_constants.end(), std::format("Push constant block '{}' not found in shader", block_name));

	const auto& block = *it;
	assert(offset + block.size <= 128, "Push constant default limit is 128 bytes");

	command.pushConstants(
		layout,
		it->stage_flags,
		static_cast<std::uint32_t>(offset),
		block.size,
		data
	);
}

auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>& values) const -> void {
	const auto it = std::ranges::find_if(
		m_push_constants, 
		[&](const auto& b) {
			return b.name == block_name;
		}
	);

	assert(it != m_push_constants.end(), std::format("Push constant block '{}' not found in shader", block_name));

	const auto& block = *it;
	std::vector buffer(block.size, std::byte{ 0 });

	for (const auto& [name, data_span] : values) {
		auto member_it = block.members.find(name);

		assert(
			member_it != block.members.end(),
			std::format("Member '{}' not found in push constant block '{}'", name, block_name)
		);

		const auto& member_info = member_it->second;

		assert(
			data_span.size() <= member_info.size,
			std::format("Provided bytes for '{}' (size {}) exceed member size ({})", member_info.name, data_span.size(), member_info.size)
		);

		std::memcpy(buffer.data() + member_info.offset, data_span.data(), data_span.size());
	}

	command.pushConstants(
		layout,
		it->stage_flags,
		0,
		static_cast<std::uint32_t>(buffer.size()),
		buffer.data()
	);
}

auto gse::shader::descriptor_set(const vk::raii::Device& device, const vk::DescriptorPool pool, const set::binding_type type, const std::uint32_t count) const -> std::vector<vk::raii::DescriptorSet> {
	assert(
		m_layout.sets.contains(type),
		std::format(
			"Shader does not have set of type {}",
			static_cast<int>(type)
		)
	);

	const auto& set_info = layout(m_layout.sets.at(type));
	assert(
		set_info,
		"Descriptor set layout is not initialized"
	);

	const vk::DescriptorSetAllocateInfo alloc_info{
		.descriptorPool = pool,
		.descriptorSetCount = count,
		.pSetLayouts = &set_info
	};

	return device.allocateDescriptorSets(alloc_info);
}

auto gse::shader::descriptor_set(const vk::raii::Device& device, const vk::DescriptorPool pool, const set::binding_type type) const -> vk::raii::DescriptorSet {
	auto sets = descriptor_set(device, pool, type, 1);
	assert(!sets.empty(), "Failed to allocate descriptor set for shader layout");
	return std::move(sets.front());
}