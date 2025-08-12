module;

#include <slang.h>

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

		enum class descriptor_layout : std::uint8_t {
			standard_3d = 0,
			deferred_3d = 1,
			forward_3d = 2,
			forward_2d = 3,
			post_process = 4,
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

		static auto generate_global_layouts(const vk::raii::Device& device) -> void;
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
	const auto root_path = config::resource_path / "Shaders";
	const auto destination_path = config::baked_resource_path / "Shaders";

	auto write_string = [](
		std::ofstream& stream,
		const std::string& str
		) {
		const std::uint32_t size = static_cast<std::uint32_t>(str.size());
			stream.write(reinterpret_cast<const char*>(&size), sizeof(size));
			stream.write(str.c_str(), size);
		};

	auto write_data = [](
		std::ofstream& stream, 
		const auto& value
		) {
			stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
		};

	slang::IGlobalSession* slang_session = nullptr;
	createGlobalSession(&slang_session);
	assert(slang_session, "Failed to create Slang global session");

	std::set<std::filesystem::path> compiled_assets;

	for (const auto& entry : std::filesystem::recursive_directory_iterator(root_path)) {
		if (!entry.is_regular_file() || entry.path().extension() != ".slang") {
			continue;
		}

		const std::string filename = entry.path().filename().string();
		if (filename.ends_with("_layout.slang") || filename == "common.slang") {
			continue;
		}

		const auto source_path = entry.path();
		const std::string base_name = source_path.stem().string();
		const auto dest_asset_file = destination_path / (base_name + ".gshader");

		compiled_assets.insert(dest_asset_file);

		if (exists(dest_asset_file) && last_write_time(source_path) <= last_write_time(dest_asset_file)) {
			continue;
		}
		if (auto dst_dir = dest_asset_file.parent_path(); !exists(dst_dir)) {
			create_directories(dst_dir);
		}

		slang::ICompileRequest* request = slang_session->createCompileRequest();
		spSetCodeGenTarget(request, SLANG_SPIRV);
		spAddSearchPath(request, root_path.string().c_str());
		spSetMatrixLayoutMode(request, SLANG_MATRIX_LAYOUT_ROW_MAJOR);

		int translation_unit = spAddTranslationUnit(request, SLANG_SOURCE_LANGUAGE_SLANG, nullptr);
		spAddTranslationUnitSourceFile(request, translation_unit, source_path.string().c_str());

		int vert_idx = spAddEntryPoint(request, translation_unit, "vs_main", SLANG_STAGE_VERTEX);
		int frag_idx = spAddEntryPoint(request, translation_unit, "fs_main", SLANG_STAGE_FRAGMENT);

		if (SLANG_FAILED(spCompile(request))) {
			assert(false, std::format("Slang compilation failed for {}:\n{}", filename, spGetDiagnosticOutput(request)));
		}

		slang::reflection::ShaderReflection* reflection = spGetReflection(request);
		vertex_input reflected_vertex_input;
		std::map<set::binding_type, set> reflected_sets;
		std::vector<struct uniform_block> reflected_pcs;

		if (auto* vert_entry = reflection->getEntryPointByIndex(vert_idx)) {
			for (std::uint32_t i = 0; i < vert_entry->getParameterCount(); ++i) {
				if (auto* param = vert_entry->getParameterByIndex(i); param->getCategory() == slang::reflection::ParameterCategory::VaryingInput) {
					reflected_vertex_input.attributes.push_back({
						.location = param->getLocation(),
						.binding = 0,
						.format = static_cast<vk::Format>(param->getTypeLayout()->getFormat()),
						.offset = 0
					});
				}
			}
		}

		for (std::uint32_t i = 0; i < reflection->getGlobalParamsCount(); ++i) {
			auto* param = reflection->getGlobalParamByIndex(i);
			auto* type_layout = param->getTypeLayout();
			auto set_type = static_cast<set::binding_type>(param->getBindingSpace());

			auto& current_set = reflected_sets[set_type];
			current_set.type = set_type;

			vk::DescriptorSetLayoutBinding layout_binding = {
				.binding = param->getBindingIndex(),
				.descriptorType = static_cast<vk::DescriptorType>(type_layout->getDescriptorType()),
				.descriptorCount = (type_layout->getKind() == slang::reflection::TypeKind::Array) ? static_cast<std::uint32_t>(type_layout->getElementCount()) : 1u,
				.stageFlags = vk::ShaderStageFlags{}
			};

			for (std::uint32_t e_idx = 0; e_idx < reflection->getEntryPointCount(); ++e_idx) {
				if (reflection->getEntryPointByIndex(e_idx)->isGlobalParamUsed(param)) {
					layout_binding.stageFlags |= static_cast<vk::ShaderStageFlagBits>(reflection->getEntryPointByIndex(e_idx)->getStage());
				}
			}

			bool found = false;
			for (auto& existing : current_set.bindings) {
				if (existing.layout_binding.binding == layout_binding.binding) {
					existing.layout_binding.stageFlags |= layout_binding.stageFlags;
					found = true;
					break;
				}
			}

			if (!found) {
				std::optional<struct uniform_block> block_member;
				if (type_layout->getKind() == slang::reflection::TypeKind::ParameterBlock) {
					struct uniform_block b = {
						.name = param->getName(),
						.binding = layout_binding.binding,
						.set = static_cast<std::uint32_t>(set_type),
						.size = static_cast<std::uint32_t>(type_layout->getSize())
					};
					for (std::uint32_t m = 0; m < type_layout->getFieldCount(); ++m) {
						auto* member = type_layout->getFieldByIndex(m);
						b.members[member->getName()] = uniform_member{
							.name = member->getName(),
							.offset = static_cast<std::uint32_t>(member->getOffset()),
							.size = static_cast<std::uint32_t>(member->getTypeLayout()->getSize())
						};
					}
					block_member = b;
				}
				current_set.bindings.emplace_back(param->getName(), layout_binding, block_member);
			}
		}

		if (auto* layout = reflection->getLayout()) {
			if (layout->getPushConstantRangeCount() > 0) {
				auto* pcr = layout->getPushConstantRangeByIndex(0);
				auto* pcr_type = pcr->getTypeLayout();
				struct uniform_block b = {
					.name = pcr_type->getName() ? pcr_type->getName() : "push_constants",
					.size = static_cast<std::uint32_t>(pcr_type->getSize())
				};
				for (std::uint32_t m = 0; m < pcr_type->getFieldCount(); ++m) {
					auto* member = pcr_type->getFieldByIndex(m);
					b.members[member->getName()] = uniform_member{
						.name = member->getName(),
						.offset = static_cast<std::uint32_t>(member->getOffset()),
						.size = static_cast<std::uint32_t>(member->getTypeLayout()->getSize())
					};
				}
				reflected_pcs.push_back(b);
			}
		}

		std::ofstream out(dest_asset_file, std::ios::binary);
		assert(out.is_open(), std::format("Failed to create gshader file: {}", dest_asset_file.string()));

		write_data(out, static_cast<std::uint32_t>(reflected_vertex_input.attributes.size()));
		for (const auto& attr : reflected_vertex_input.attributes) write_data(out, attr);

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
					}
				}
			}
		}

		write_data(out, static_cast<std::uint32_t>(reflected_pcs.size()));
		for (const auto& pc : reflected_pcs) {
			write_string(out, pc.name);
			write_data(out, pc.size);
			write_data(out, static_cast<std::uint32_t>(pc.members.size()));
			for (const auto& [m_name, m_data] : pc.members) {
				write_string(out, m_name);
				write_string(out, m_data.name);
				write_data(out, m_data.offset);
				write_data(out, m_data.size);
			}
		}

		// Write SPIR-V bytecode
		size_t vert_size = 0, frag_size = 0;
		const void* vert_data = spGetEntryPointCode(request, vert_idx, &vert_size);
		const void* frag_data = spGetEntryPointCode(request, frag_idx, &frag_size);
		write_data(out, vert_size);
		out.write(static_cast<const char*>(vert_data), vert_size);
		write_data(out, frag_size);
		out.write(static_cast<const char*>(frag_data), frag_size);

		std::cout << "Baked shader asset: " << dest_asset_file.filename().string() << "\n";
		spDestroyCompileRequest(request);
	}

	slang_session->release();
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

	std::uint32_t attr_count;
	read_data(in, attr_count);
	m_vertex_input.attributes.resize(attr_count);
	for (auto& attr : m_vertex_input.attributes) read_data(in, attr);

	std::uint32_t num_sets;
	read_data(in, num_sets);
	for (std::uint32_t i = 0; i < num_sets; ++i) {
		set::binding_type type;
		read_data(in, type);
		std::uint32_t num_bindings;
		read_data(in, num_bindings);
		std::vector<struct binding> bindings;
		for (std::uint32_t j = 0; j < num_bindings; ++j) {
			struct binding b;
			read_string(in, b.name);
			read_data(in, b.layout_binding);
			bool has_member;
			read_data(in, has_member);
			if (has_member) {
				struct uniform_block member;
				read_string(in, member.name);
				read_data(in, member.binding);
				read_data(in, member.set);
				read_data(in, member.size);
				std::uint32_t num_ubo_members;
				read_data(in, num_ubo_members);
				for (std::uint32_t k = 0; k < num_ubo_members; ++k) {
					std::string key_name;
					uniform_member ubo_member;
					read_string(in, key_name);
					read_string(in, ubo_member.name);
					read_data(in, ubo_member.offset);
					read_data(in, ubo_member.size);
					member.members[key_name] = ubo_member;
				}
				b.member = member;
			}
			bindings.push_back(std::move(b));
		}
		m_layout.sets[type] = { .type = type, .bindings = std::move(bindings) };
	}

	std::uint32_t pc_count;
	read_data(in, pc_count);
	m_push_constants.resize(pc_count);
	for (auto& pc : m_push_constants) {
		read_string(in, pc.name);
		read_data(in, pc.size);
		std::uint32_t member_count;
		read_data(in, member_count);
		for (std::uint32_t i = 0; i < member_count; ++i) {
			std::string key, name;
			std::uint32_t offset, size;
			read_string(in, key);
			read_string(in, name);
			read_data(in, offset);
			read_data(in, size);
			pc.members[key] = { .name = name, .offset = offset, .size = size };
		}
	}

	size_t vert_size, frag_size;
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
		auto get_format_size = [](
			const vk::Format format
			) -> std::uint32_t {
				switch (format) {
					case vk::Format::eR32Sfloat: return 4;
					case vk::Format::eR32G32Sfloat: return 8;
					case vk::Format::eR32G32B32Sfloat: return 12;
					case vk::Format::eR32G32B32A32Sfloat: return 16;
					default: assert(false, "Unsupported vertex format"); return 0;
				}
			};
		for (auto& attr : m_vertex_input.attributes) {
			attr.offset = stride;
			stride += get_format_size(attr.format);
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
		for (const auto& b : set_data.bindings) raw_bindings.push_back(b.layout_binding);

		vk::DescriptorSetLayoutCreateInfo ci{
			.flags = type == set::binding_type::push ? vk::DescriptorSetLayoutCreateFlagBits::ePushDescriptorKHR : vk::DescriptorSetLayoutCreateFlags(),
			.bindingCount = static_cast<std::uint32_t>(raw_bindings.size()),
			.pBindings = raw_bindings.data()
		};
		set_data.layout = context.config().device_config().device.createDescriptorSetLayout(ci);
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

auto gse::shader::push_constant_range(const std::string_view& name, const vk::ShaderStageFlags stages) const -> vk::PushConstantRange {
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

			auto mem_it = ub.members.find(member_name);

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
\
auto gse::shader::set_uniform_block(const std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>& data, const vulkan::persistent_allocator::allocation& alloc) const -> void {
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
		stages,
		static_cast<std::uint32_t>(offset),
		block.size,
		data
	);
}

auto gse::shader::push(const vk::CommandBuffer command, const vk::PipelineLayout layout, std::string_view block_name, const std::unordered_map<std::string, std::span<const std::byte>>& values, const vk::ShaderStageFlags stages) const -> void {
	const auto it = std::ranges::find_if(
		m_push_constants, 
		[&](const auto& b) {
			return b.name == block_name;
		});

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
		stages,
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

auto gse::shader::generate_global_layouts(const vk::raii::Device& device) -> void {
	assert(
		global_layouts.empty(),
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
					.bindingCount = static_cast<std::uint32_t>(bindings.size()),
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

	global_layouts.clear();

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

	global_layouts.emplace(descriptor_layout::standard_3d, std::move(std_3d));

	struct layout def_3d;

	create_layout(def_3d, set::binding_type::persistent, {
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1,			fs }, // Albedo
		{ 1, vk::DescriptorType::eCombinedImageSampler, 1,			fs }, // Normal
		{ 2, vk::DescriptorType::eCombinedImageSampler, 1,			fs }, // Depth
		{ 3, vk::DescriptorType::eUniformBuffer,        1,			fs }, // Cam
		{ 4, vk::DescriptorType::eStorageBuffer,		1,			fs },
		{ 5, vk::DescriptorType::eCombinedImageSampler, 1,			fs },
		{ 6, vk::DescriptorType::eCombinedImageSampler, 1,			fs },
		});

	create_layout(def_3d, set::binding_type::push, {
		{ 0, vk::DescriptorType::eUniformBuffer,        1, vs },
	});

	global_layouts.emplace(descriptor_layout::deferred_3d, std::move(def_3d));

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

	global_layouts.emplace(descriptor_layout::forward_3d, std::move(for_3d));

	struct layout for_2d;

	create_layout(for_2d, set::binding_type::persistent, {});

	create_layout(for_2d, set::binding_type::push, {
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },
	});

	global_layouts.emplace(descriptor_layout::forward_2d, std::move(for_2d));

	struct layout post_process;

	create_layout(post_process, set::binding_type::persistent, {
		{ 0, vk::DescriptorType::eCombinedImageSampler, 1, fs },
		{ 1, vk::DescriptorType::eCombinedImageSampler, 1, fs },
	});

	global_layouts.emplace(descriptor_layout::post_process, std::move(post_process));
}


