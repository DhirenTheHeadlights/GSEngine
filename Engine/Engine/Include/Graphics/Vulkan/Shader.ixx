module;

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <SPIRV-Reflect/spirv_reflect.h>

export module gse.graphics:shader;

import std;

import :rendering_context;

import gse.assert;
import gse.platform;

namespace gse {
	export class shader final : public identifiable, non_copyable {
	public:
		struct info {
			std::string name;
			std::filesystem::path vert_path;
			std::filesystem::path frag_path;
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
			std::vector<uniform_member> members;
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
			std::variant<std::monostate, vk::raii::DescriptorSetLayout, vk::DescriptorSetLayout> layout;
			std::vector<binding> bindings;
		};

		struct layout {
			std::unordered_map<set::binding_type, set> sets;
		};

		struct handle {
			handle(shader& sh) : shader(&sh) {}
			shader* shader = nullptr;
		};

		enum class descriptor_layout : std::uint8_t {
			standard_3d = 0,
			deferred_3d = 1,
			forward_3d = 2,
			forward_2d = 3,
			post_process = 4,
			custom = 99
		};

		shader(const std::filesystem::path& path);

		auto load(
			renderer::context& context
		) -> void;

		static auto generate_global_layouts(const vk::raii::Device& device) -> void;

		auto unload() -> void;

		auto shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
		auto required_bindings() const -> std::vector<std::string>;
		auto uniform_block(const std::string& name) const -> uniform_block;
		auto uniform_blocks() const -> std::vector<class uniform_block>;
		auto binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding>;
		auto layout(set::binding_type type) const -> vk::DescriptorSetLayout;
		auto layouts() const -> std::vector<vk::DescriptorSetLayout>;
		auto push_constant_range(const std::string_view& name, vk::ShaderStageFlags stages) const -> vk::PushConstantRange;
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

		template <typename T>
		auto set_uniform_block(
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& data,
			const vulkan::persistent_allocator::allocation& alloc
		) const -> void;

		auto push(
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
			std::size_t offset = 0,
			vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eVertex
		) const -> void;

		auto push(
			vk::CommandBuffer command,
			vk::PipelineLayout layout,
			std::string_view block_name,
			const std::unordered_map<std::string, std::span<const std::byte>>& values,
			vk::ShaderStageFlags stages = vk::ShaderStageFlagBits::eVertex
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
			if (std::holds_alternative<vk::raii::DescriptorSetLayout>(set.layout)) {
				return *std::get<vk::raii::DescriptorSetLayout>(set.layout);
			}
			if (std::holds_alternative<vk::DescriptorSetLayout>(set.layout)) {
				return std::get<vk::DescriptorSetLayout>(set.layout);
			}
			assert(false, "Shader set layout is not initialized");
			return {};
		}

		static std::unordered_map<descriptor_layout, struct layout> m_global_layouts;
	};

	auto read_file(const std::filesystem::path& path) -> std::vector<char>;
}

gse::shader::shader(const std::filesystem::path& path) : identifiable(path.stem().string()) {
	const auto dir = path.parent_path();
	const auto stem = path.stem().string();

	m_info.name = stem;
	m_info.vert_path = dir / (stem + ".vert.spv");
	m_info.frag_path = dir / (stem + ".frag.spv");

	assert(
		exists(m_info.vert_path) && exists(m_info.frag_path),
		std::format("Shader files not found: {} and {}", m_info.vert_path.string(), m_info.frag_path.string())
	);
}


auto gse::shader::load(renderer::context& context) -> void {
	if (m_global_layouts.empty()) {
		generate_global_layouts(context.config().device_data.device);
	}

	const auto parse = [&](
		const std::filesystem::path& path
		) -> std::pair<std::filesystem::path, descriptor_layout> {
			glslang::InitializeProcess();

			const auto root_path = config::shader_raw_path;
			const auto destination_path = config::shader_spirv_path;

			const auto ext = path.extension().string();
			if (ext != ".vert" && ext != ".frag" && ext != ".comp" && ext != ".geom" && ext != ".tesc" && ext != ".tese") return {};

			auto layout = descriptor_layout::custom;

			if (ext == ".vert" || ext == ".frag") {
				/// Grab descriptor layout type. Shader sets in this format: 'layout (constant_id = 99) const int descriptor_layout_type = 1;'
				std::ifstream file(path);
				std::string line;

				const std::string token = "const int descriptor_layout_type =";

				while (std::getline(file, line)) {
					if (auto pos = line.find(token); pos != std::string::npos) {
						pos += token.size();
						auto end = line.find(';', pos);
						std::string value_str = line.substr(pos, end - pos);
						value_str.erase(std::ranges::remove_if(value_str, isspace).begin(), value_str.end());
						int layout_value = std::stoi(value_str);
						layout = static_cast<descriptor_layout>(layout_value);
						break;
					}
				}
			}

			const auto source_path = path;
			const auto destination_relative = relative(source_path, root_path);
			const auto destination_file = destination_path / (destination_relative.string() + ".spv");

			if (exists(destination_file)) {
				auto src_time = last_write_time(source_path);
				auto dst_time = last_write_time(destination_file);
				if (src_time <= dst_time) {
					std::cout << "Shader already compiled: " << destination_file.filename().string() << '\n';
				}
			}
			else {
				if (auto dst_dir = destination_file.parent_path(); !exists(dst_dir)) {
					create_directories(dst_dir);
				}
			}

			EShLanguage stage;

			if (ext == ".vert") stage = EShLangVertex;
			else if (ext == ".frag") stage = EShLangFragment;
			else if (ext == ".comp") stage = EShLangCompute;
			else if (ext == ".geom") stage = EShLangGeometry;
			else if (ext == ".tesc") stage = EShLangTessControl;
			else if (ext == ".tese") stage = EShLangTessEvaluation;
			else assert(false, std::format("Unknown shader extension: {}", ext));

			std::ifstream in(source_path, std::ios::ate | std::ios::binary);
			assert(in.is_open(), std::format("Failed to open shader file: {}", source_path.string().c_str()));

			size_t n = in.tellg();
			std::string source(n, '\0');
			in.seekg(0);
			in.read(source.data(), n);

			if (source.size() > 3 && source[0] == 0xEF && source[1] == 0xBB && source[2] == 0xBF) {
				source.erase(0, 3);
			}

			const char* code = source.c_str();

			glslang::TShader shader(stage);

			shader.setStrings(&code, 1);
			shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
			shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_2);
			shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_5);

			bool ok = shader.parse(
				GetDefaultResources(),
				450,
				false,
				EShMsgDefault
			);

			if (!ok) {
				auto info = shader.getInfoLog();
				auto debug = shader.getInfoDebugLog();

				auto msg = std::format(
					"File ({}) GLSL parse error:\n{}\n{}",
					source_path.filename().string(), info, debug
				);
				assert(false, msg);
			}

			glslang::TProgram program;
			program.addShader(&shader);
			assert(program.link(EShMsgDefault), std::format("GLSL link error:\n{}", program.getInfoLog()));

			std::vector<uint32_t> spirv;
			GlslangToSpv(*program.getIntermediate(stage), spirv);

			std::ofstream out(destination_file, std::ios::binary);
			assert(out.is_open(), std::format("Failed to write compiled SPIR-V: {}", destination_file.string().c_str()));
			out.write(reinterpret_cast<const char*>(spirv.data()), spirv.size() * sizeof(uint32_t));

			glslang::FinalizeProcess();

			return { destination_file, layout };
		};

	const auto [vert_parse_path, layout_type] = parse(m_info.vert_path);
	const auto [frag_parse_path, layout_type_check] = parse(m_info.frag_path);

	assert(
		layout_type == layout_type_check,
		std::format(
			"Shader layout type mismatch: {} vs {}",
			static_cast<std::uint8_t>(layout_type),
			static_cast<std::uint8_t>(layout_type_check)
		)
	);

	m_info = {
		.name = m_info.name,
		.vert_path = vert_parse_path,
		.frag_path = frag_parse_path
	};

	const auto vert_code = read_file(m_info.vert_path);
	const auto frag_code = read_file(m_info.frag_path);

	auto make_module = [&](const std::vector<char>& code) {
		const vk::ShaderModuleCreateInfo ci{
			.flags = {},
			.codeSize = code.size(),
			.pCode = reinterpret_cast<const uint32_t*>(code.data())
		};
		return context.config().device_data.device.createShaderModule(ci);
		};

	m_vert_module = make_module(vert_code);
	m_frag_module = make_module(frag_code);

	SpvReflectShaderModule vertex_input_reflect_module;
	auto vertex_input_reflect_module_result = spvReflectCreateShaderModule(vert_code.size(), vert_code.data(), &vertex_input_reflect_module);
	assert(vertex_input_reflect_module_result == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection for vertex inputs failed");

	uint32_t vertex_input_count = 0;
	spvReflectEnumerateInputVariables(&vertex_input_reflect_module, &vertex_input_count, nullptr);
	std::vector<SpvReflectInterfaceVariable*> inputs(vertex_input_count);
	spvReflectEnumerateInputVariables(&vertex_input_reflect_module, &vertex_input_count, inputs.data());

	m_vertex_input.attributes.clear();
	m_vertex_input.bindings.clear();

	for (const auto* var : inputs) {
		if (var->storage_class == SpvStorageClassInput && var->built_in == -1) {
			m_vertex_input.attributes.push_back(vk::VertexInputAttributeDescription{
				.location = var->location,
				.binding = 0, 
				.format = static_cast<vk::Format>(var->format),
				.offset = 0 
				}
			);
		}
	}

	std::ranges::sort(m_vertex_input.attributes, {}, &vk::VertexInputAttributeDescription::location);

	uint32_t current_offset = 0;
	uint32_t stride = 0;

	auto get_format_size = [](const vk::Format format) -> uint32_t {
		switch (format) {
		case vk::Format::eR32Sfloat:          return 4;
		case vk::Format::eR32G32Sfloat:       return 8;
		case vk::Format::eR32G32B32Sfloat:    return 12;
		case vk::Format::eR32G32B32A32Sfloat: return 16;
			// Add other formats
		default:
			assert(false, "Unsupported vertex format for size calculation");
			return 0;
		}
		};

	for (auto& attr : m_vertex_input.attributes) {
		attr.offset = current_offset;
		uint32_t format_size = get_format_size(attr.format);
		current_offset += format_size;
	}

	stride = current_offset;

	if (!m_vertex_input.attributes.empty()) {
		m_vertex_input.bindings.push_back(vk::VertexInputBindingDescription{
			.binding = 0,
			.stride = stride,
			.inputRate = vk::VertexInputRate::eVertex
			});
	}

	spvReflectDestroyShaderModule(&vertex_input_reflect_module);

	struct reflected_binding {
		std::string name;
		vk::DescriptorSetLayoutBinding layout_binding;
		std::uint32_t set_index;
	};
	std::vector<reflected_binding> all_reflected;

	auto reflect_descriptors = [&](const std::vector<char>& spirv) {
		SpvReflectShaderModule reflect_module;
		const auto result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &reflect_module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection failed");

		uint32_t count = 0;
		spvReflectEnumerateDescriptorBindings(&reflect_module, &count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> reflected(count);
		spvReflectEnumerateDescriptorBindings(&reflect_module, &count, reflected.data());

		for (auto* b : reflected) {
			all_reflected.push_back(reflected_binding{
				.name = b->name ? b->name : "",
				.layout_binding = vk::DescriptorSetLayoutBinding{
					b->binding,
					static_cast<vk::DescriptorType>(b->descriptor_type),
					1u,
					static_cast<vk::ShaderStageFlagBits>(reflect_module.shader_stage)
				},
				.set_index = b->set
				});
		}

		spvReflectDestroyShaderModule(&reflect_module);
		};

	reflect_descriptors(vert_code);
	reflect_descriptors(frag_code);

	auto reflect_uniforms = [&](const std::vector<char>& spirv) {
		std::vector<struct uniform_block> blocks;
		SpvReflectShaderModule reflect_module;
		const auto result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &reflect_module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection failed");

		uint32_t count = 0;
		spvReflectEnumerateDescriptorBindings(&reflect_module, &count, nullptr);
		std::vector<SpvReflectDescriptorBinding*> reflected(count);
		spvReflectEnumerateDescriptorBindings(&reflect_module, &count, reflected.data());

		for (const auto* b : reflected) {
			if (b->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
				struct uniform_block ub {
					.name = b->name,
						.binding = b->binding,
						.set = b->set,
						.size = b->block.size
				};
				for (uint32_t i = 0; i < b->block.member_count; ++i) {
					auto& mem = b->block.members[i];
					ub.members.emplace_back(
						mem.name,
						mem.type_description->type_name ? mem.type_description->type_name : "",
						mem.offset,
						mem.size,
						mem.array.dims_count ? mem.array.dims[0] : 0
					);
				}
				blocks.push_back(std::move(ub));
			}
		}

		spvReflectDestroyShaderModule(&reflect_module);
		return blocks;
		};

	auto blocks = reflect_uniforms(vert_code);
	auto more_blocks = reflect_uniforms(frag_code);
	blocks.insert(blocks.end(), more_blocks.begin(), more_blocks.end());

	auto reflect_push_constants = [&](const std::vector<char>& spirv) {
		std::vector<struct uniform_block> pc_blocks;
		SpvReflectShaderModule reflect_module;
		const auto result = spvReflectCreateShaderModule(spirv.size(), spirv.data(), &reflect_module);
		assert(result == SPV_REFLECT_RESULT_SUCCESS, "SPIR-V reflection for push constants failed");

		uint32_t count = 0;
		spvReflectEnumeratePushConstantBlocks(&reflect_module, &count, nullptr);
		if (count > 0) {
			std::vector<SpvReflectBlockVariable*> reflected(count);
			spvReflectEnumeratePushConstantBlocks(&reflect_module, &count, reflected.data());

			for (const auto* block_var : reflected) {
				struct uniform_block pcb {
					.name = block_var->name ? block_var->name : "push_constants",
						.binding = 0, // Not applicable
						.set = 0,     // Not applicable
						.size = block_var->size
				};
				for (uint32_t i = 0; i < block_var->member_count; ++i) {
					const auto& mem = block_var->members[i];
					pcb.members.emplace_back(
						mem.name,
						mem.type_description->type_name ? mem.type_description->type_name : "",
						mem.offset,
						mem.size,
						mem.array.dims_count ? mem.array.dims[0] : 0
					);
				}
				pc_blocks.push_back(std::move(pcb));
			}
		}
		spvReflectDestroyShaderModule(&reflect_module);
		return pc_blocks;
		};

	m_push_constants.clear();
	auto vert_pcs = reflect_push_constants(vert_code);
	auto frag_pcs = reflect_push_constants(frag_code);
	m_push_constants.insert(m_push_constants.end(), std::make_move_iterator(vert_pcs.begin()), std::make_move_iterator(vert_pcs.end()));
	m_push_constants.insert(m_push_constants.end(), std::make_move_iterator(frag_pcs.begin()), std::make_move_iterator(frag_pcs.end()));

	if (!m_push_constants.empty()) {
		std::ranges::sort(m_push_constants, {}, &uniform_block::name);
		auto [new_end, _] = std::ranges::unique(m_push_constants, {}, &uniform_block::name);
		m_push_constants.erase(new_end, m_push_constants.end());
	}

	std::unordered_map<set::binding_type, std::vector<struct binding>> grouped;
	for (auto& [name, layout_binding, set_index] : all_reflected) {
		auto type = static_cast<set::binding_type>(set_index);
		std::optional<struct uniform_block> block_info;

		if (layout_binding.descriptorType == vk::DescriptorType::eUniformBuffer) {
			auto it = std::ranges::find_if(
				blocks,
				[&](auto& ub) {
					return
						ub.binding == layout_binding.binding
						&& ub.set == set_index;
				}
			);
			if (it != blocks.end()) {
				block_info = *it;
			}
		}

		grouped[type].emplace_back(
			name,
			layout_binding,
			block_info
		);
	}

	if (!grouped.empty() && !grouped.contains(set::binding_type::persistent)) {
		grouped.emplace(set::binding_type::persistent, std::vector<struct binding>{});
	}

	if (const auto it = m_global_layouts.find(layout_type); it != m_global_layouts.end()) {
		for (const auto& [type, user_set] : it->second.sets) {
			std::vector<struct binding> complete_bindings;

			auto reflected_in_set = all_reflected | std::views::filter([&](const auto& r) {
				return static_cast<set::binding_type>(r.set_index) == type;
				});

			for (const auto& [name, layout_binding, set_index] : reflected_in_set) {
				auto it2 = std::ranges::find_if(user_set.bindings, [&](const auto& b) {
					return b.layout_binding.binding == layout_binding.binding;
					});

				assert(
					it2 != user_set.bindings.end(),
					std::format(
						"Reflected binding '{}' (at binding {}) not found in provided layout for set {}",
						name,
						layout_binding.binding,
						static_cast<int>(type)
					)
				);

				assert(it2->layout_binding.descriptorType == layout_binding.descriptorType,
					std::format(
						"Descriptor type mismatch for '{}' in set {}",
						name,
						static_cast<int>(type)
					)
				);

				std::optional<struct uniform_block> block_info;
				if (layout_binding.descriptorType == vk::DescriptorType::eUniformBuffer) {
					auto block_it = std::ranges::find_if(blocks, [&](const auto& ub) {
						return ub.binding == layout_binding.binding && ub.set == set_index;
						});
					if (block_it != blocks.end()) {
						block_info = *block_it;
					}
				}

				complete_bindings.emplace_back(
					name,
					layout_binding,
					std::move(block_info)
				);
			}

			vk::DescriptorSetLayout non_owning_handle = *std::get<vk::raii::DescriptorSetLayout>(user_set.layout);

			m_layout.sets.emplace(
				type,
				set{
					.type = type,
					.layout = non_owning_handle,
					.bindings = std::move(complete_bindings)
				}
			);
		}
	}
	else {
		for (auto& [type, binds] : grouped) {
			std::vector<vk::DescriptorSetLayoutBinding> raw;
			raw.reserve(binds.size());
			for (auto& b : binds) raw.push_back(b.layout_binding);

			vk::DescriptorSetLayoutCreateInfo ci{
				.flags = {},
				.bindingCount = static_cast<uint32_t>(raw.size()),
				.pBindings = raw.data()
			};
			auto raii_layout = context.config().device_data.device.createDescriptorSetLayout(ci);

			m_layout.sets.emplace(
				type,
				set{
					.type = type,
					.layout = std::move(raii_layout),
					.bindings = std::move(binds)
				}
			);
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

auto gse::shader::shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2> {
	return {
		vk::PipelineShaderStageCreateInfo{
			.flags = {},
			.stage = vk::ShaderStageFlagBits::eVertex,
			.module = *m_vert_module,
			.pName = "main"
		},
		vk::PipelineShaderStageCreateInfo{
			.flags = {},
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

auto gse::shader::uniform_block(const std::string & name) const -> class uniform_block {
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

auto gse::shader::binding(const std::string & name) const -> std::optional<vk::DescriptorSetLayoutBinding> {
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

auto gse::shader::push_constant_range(const std::string_view & name, const vk::ShaderStageFlags stages) const -> vk::PushConstantRange {
	const auto it = std::ranges::find_if(m_push_constants, [&](const auto& block) {
		return block.name == name;
		});

	assert(it != m_push_constants.end(), std::format("Push constant block '{}' not found in shader", name));

	return vk::PushConstantRange{
		stages,
		0,      
		it->size
	};
}

auto gse::shader::vertex_input_state() const -> vk::PipelineVertexInputStateCreateInfo {
	return {
		.flags = {},
		.vertexBindingDescriptionCount = static_cast<uint32_t>(m_vertex_input.bindings.size()),
		.pVertexBindingDescriptions = m_vertex_input.bindings.data(),
		.vertexAttributeDescriptionCount = static_cast<uint32_t>(m_vertex_input.attributes.size()),
		.pVertexAttributeDescriptions = m_vertex_input.attributes.data()
	};
}

auto gse::shader::descriptor_writes(const vk::DescriptorSet set, const std::unordered_map<std::string, vk::DescriptorBufferInfo>&buffer_infos, const std::unordered_map<std::string, vk::DescriptorImageInfo>&image_infos) const -> std::vector<vk::WriteDescriptorSet> {
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
auto gse::shader::set_uniform(std::string_view full_name, const T & value, const vulkan::persistent_allocator::allocation & alloc) const -> void {
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

			auto mem_it = std::find_if(
				ub.members.begin(),
				ub.members.end(),
				[&](auto& m) {
					return m.name == member_name;
				}
			);

			assert(
				mem_it != ub.members.end(),
				std::format(
					"Member '{}' not found in block '{}'",
					member_name,
					block_name
				)
			);

			const auto& mem_info = *mem_it;

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

template <typename T>
auto gse::shader::set_uniform_block(const std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>&data, const vulkan::persistent_allocator::allocation & alloc) const -> void {
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
	assert(block_binding, std::format("Uniform block '{}' not found", block_name));

	const auto& block = *block_binding->member;
	assert(alloc.mapped(), "Attempted to set uniform block but memory is not mapped");

	for (const auto& [name, bytes] : data) {
		const auto member_it = std::find_if(
			block.members.begin(), block.members.end(),
			[&](const auto& m) { return m.name == name; }
		);

		assert(
			member_it != block.members.end(),
			std::format(
				"Uniform member '{}' not found in block '{}'",
				name,
				block_name
			)
		);

		assert(
			bytes.size() <= member_it->size,
			std::format(
				"Data size {} > member size {} for '{}.{}'",
				bytes.size(),
				member_it->size,
				block_name,
				name
			)
		);

		assert(
			alloc.mapped(),
			std::format(
				"Memory not mapped for '{}.{}'",
				block_name,
				name
			)
		);

		std::memcpy(
			static_cast<std::byte*>(alloc.mapped()) + member_it->offset,
			bytes.data(),
			bytes.size()
		);
	}
}

auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, const std::string_view name, const vk::DescriptorImageInfo& image_info) const -> void {
	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [member_name, layout_binding, member] : s.bindings) {
			if (member_name == name) {
				assert(layout_binding.descriptorType == vk::DescriptorType::eCombinedImageSampler, std::format("Binding '{}' is not a combined image sampler", name));
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
							.pImageInfo = &image_info,
						}
					}
				);
				return;
			}
		}
	}
	assert(false, std::format("Binding '{}' not found in shader", name));
}

auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, const void* data, const std::size_t offset, const vk::ShaderStageFlags stages) const -> void {
	const auto it = std::ranges::find_if(m_push_constants, [&](const auto& block) {
		return block.name == block_name;
		});

	assert(it != m_push_constants.end(), std::format("Push constant block '{}' not found in shader", block_name));

	const auto& block = *it;
	assert(offset + block.size <= 128, "Push constant default limit is 128 bytes");

	command.pushConstants(
		layout,
		stages,
		static_cast<uint32_t>(offset),
		block.size, 
		data
	);
}

auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>&values, const vk::ShaderStageFlags stages) const -> void {
	const auto it = std::ranges::find_if(m_push_constants, [&](const auto& b) {
		return b.name == block_name;
		});

	assert(it != m_push_constants.end(), std::format("Push constant block '{}' not found in shader", block_name));

	const auto& block = *it;
	std::vector buffer(block.size, std::byte{ 0 });

	for (const auto& [name, data_span] : values) {
		auto member_it = std::ranges::find_if(block.members, [&](const auto& m) {
			return m.name == name;
			});

		assert(
			member_it != block.members.end(), 
			std::format(
				"Member '{}' not found in push constant block '{}'", 
				name, 
				block_name
			)
		);

		assert(
			data_span.size() <= member_it->size,
			std::format(
				"Provided bytes for '{}' (size {}) exceed member size ({})",
				member_it->name, data_span.size(), member_it->size
			)
		);
		std::memcpy(buffer.data() + member_it->offset, data_span.data(), data_span.size());
	}

	command.pushConstants(
		layout,
		stages,
		0,
		static_cast<uint32_t>(buffer.size()),
		buffer.data()
	);
}


auto gse::shader::descriptor_set(const vk::raii::Device & device, const vk::DescriptorPool pool, const set::binding_type type, const std::uint32_t count) const -> std::vector<vk::raii::DescriptorSet> {
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

auto gse::shader::descriptor_set(const vk::raii::Device & device, const vk::DescriptorPool pool, const set::binding_type type) const -> vk::raii::DescriptorSet {
	auto sets = descriptor_set(device, pool, type, 1);
	assert(!sets.empty(), "Failed to allocate descriptor set for shader layout");
	return std::move(sets.front());
}

auto gse::shader::generate_global_layouts(const vk::raii::Device& device) -> void {
	assert(
		m_global_layouts.empty(),
		"Global shader layouts already generated"
	);

	constexpr auto vs = vk::ShaderStageFlagBits::eVertex;
	constexpr auto fs = vk::ShaderStageFlagBits::eFragment;

	constexpr int max_lights = 10;

	auto create_layout = [&](
		struct layout& layout,
		const set::binding_type type,
		const std::vector<vk::DescriptorSetLayoutBinding>& bindings
		) -> void {
			vk::raii::DescriptorSetLayout descriptor_set_layout = device.createDescriptorSetLayout(
				vk::DescriptorSetLayoutCreateInfo{
					.flags = type == set::binding_type::push ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags(),
					.bindingCount = static_cast<uint32_t>(bindings.size()),
					.pBindings = bindings.data()
				}
			);

			std::vector<struct binding> shader_bindings;
			shader_bindings.reserve(bindings.size());

			for (const auto& binding : bindings) {
				shader_bindings.emplace_back(std::string(), binding, std::nullopt);
			}

			layout.sets[type] = {
				.type = type,
				.layout = std::move(descriptor_set_layout),
				.bindings = std::move(shader_bindings)
			};
		};

	m_global_layouts.clear();

	struct layout std_3d;

	create_layout(std_3d, set::binding_type::persistent, {
		{ 0, vk::DescriptorType::eUniformBuffer,        1, vs },
		{ 1, vk::DescriptorType::eUniformBuffer,        1, vs },
		{ 2, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		{ 3, vk::DescriptorType::eStorageBuffer,        1, fs },
		{ 4, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		{ 5, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		{ 6, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	m_global_layouts.emplace(descriptor_layout::standard_3d, std::move(std_3d));

	struct layout def_3d;

	create_layout(def_3d, set::binding_type::persistent, {
		{ 0, vk::DescriptorType::eCombinedImageSampler,      1, fs },	// g_albedo
		{ 1, vk::DescriptorType::eCombinedImageSampler,      1, fs },	// g_normal
		{ 2, vk::DescriptorType::eCombinedImageSampler,      1, fs },	// g_depth
		{ 3, vk::DescriptorType::eCombinedImageSampler, max_lights, fs },
		{ 4, vk::DescriptorType::eCombinedImageSampler, max_lights, fs },
		{ 5, vk::DescriptorType::eUniformBuffer,        1, vs | fs },	// light_space_matrix
		{ 6, vk::DescriptorType::eCombinedImageSampler, 1, fs },		// diffuse_texture
		{ 7, vk::DescriptorType::eCombinedImageSampler, 1, fs },		// environment_map
		{ 8, vk::DescriptorType::eStorageBuffer,        1, fs },		// light buffer
		});

	m_global_layouts.emplace(descriptor_layout::deferred_3d, std::move(def_3d));

	struct layout for_3d;

	create_layout(for_3d, set::binding_type::persistent, {
		{ 0, vk::DescriptorType::eUniformBuffer, 1, vs | fs },
		{ 4, vk::DescriptorType::eStorageBuffer, 1, fs }
		});

	create_layout(for_3d, set::binding_type::push, {
		{ 0, vk::DescriptorType::eUniformBuffer,        1, vs },
		{ 1, vk::DescriptorType::eUniformBuffer,        1, vs },
		{ 2, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		{ 3, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	m_global_layouts.emplace(descriptor_layout::forward_3d, std::move(for_3d));

	struct layout for_2d;

	create_layout(for_2d, set::binding_type::persistent, {});

	create_layout(for_2d, set::binding_type::push, {
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	m_global_layouts.emplace(descriptor_layout::forward_2d, std::move(for_2d));

	struct layout post_process;

	create_layout(post_process, set::binding_type::persistent, {
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		{ 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		});

	m_global_layouts.emplace(descriptor_layout::post_process, std::move(post_process));
}

auto gse::read_file(const std::filesystem::path & path) -> std::vector<char> {
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	assert(file.is_open(), "Failed to open file!");

	const size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	return buffer;
}
