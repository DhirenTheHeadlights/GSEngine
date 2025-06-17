module;

#include <SPIRV-Reflect/spirv_reflect.h>

export module gse.graphics.shader;

import std;
import vulkan_hpp;

import gse.platform;

namespace gse {
	export class shader {
	public:
		struct info {
			std::string name;
			std::filesystem::path vert_path;
			std::filesystem::path frag_path;
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

		shader() = default;
		shader(
			const vk::raii::Device& device,
			const std::filesystem::path& vert_path,
			const std::filesystem::path& frag_path, 
			const layout* layout = nullptr
		);

		shader(const shader&) = delete;
		auto operator=(const shader&) -> shader & = delete;

		auto create(
			const vk::raii::Device& device,
			const std::filesystem::path& vert_path,
			const std::filesystem::path& frag_path,
			const layout* provided_layout = nullptr
		) -> void;

		auto shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2>;
		auto required_bindings() const -> std::vector<std::string>;
		auto uniform_block(const std::string& name) const -> uniform_block;
		auto uniform_blocks() const -> std::vector<class uniform_block>;
		auto binding(const std::string& name) const -> std::optional<vk::DescriptorSetLayoutBinding>;
		auto layout(const set::binding_type type) const -> vk::DescriptorSetLayout;
		auto layouts() const -> std::vector<vk::DescriptorSetLayout>;

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

		auto descriptor_set(
			const vk::raii::Device& device,
			vk::DescriptorPool pool,
			set::binding_type type, std::uint32_t count
		) const -> std::vector<vk::raii::DescriptorSet>;

		auto descriptor_set(
			const vk::raii::Device& device,
			vk::DescriptorPool pool, set::binding_type type
		) const -> vk::raii::DescriptorSet;
	private:
		vk::raii::ShaderModule m_vert_module = nullptr;
		vk::raii::ShaderModule m_frag_module = nullptr;

		struct layout m_layout;
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
	};

	auto read_file(const std::filesystem::path& path) -> std::vector<char>;
}

gse::shader::shader(const vk::raii::Device& device, const std::filesystem::path& vert_path, const std::filesystem::path& frag_path, const struct layout* layout) : m_info{ vert_path.filename().string(), vert_path, frag_path } {
	create(device, vert_path, frag_path, layout);
}

auto gse::shader::create(
	const vk::raii::Device& device,
	const std::filesystem::path& vert_path,
	const std::filesystem::path& frag_path,
	const struct layout* provided_layout
) -> void {
	const auto vert_code = read_file(vert_path);
	const auto frag_code = read_file(frag_path);

	auto make_module = [&](const std::vector<char>& code) { 
		const vk::ShaderModuleCreateInfo ci{
			{},
			code.size(),
			reinterpret_cast<const uint32_t*>(code.data())
		};
		return device.createShaderModule(ci);
		};

	m_vert_module = make_module(vert_code);
	m_frag_module = make_module(frag_code);

	struct reflected_binding {
		std::string                          name;
		vk::DescriptorSetLayoutBinding      layout_binding;
		std::uint32_t                        set_index;
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
				struct uniform_block ub{
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

	if (provided_layout) {
		for (const auto& [type, user_set] : provided_layout->sets) {
			std::vector<struct binding> complete_bindings;

			auto reflected_in_set = all_reflected | std::views::filter([&](const auto& r) {
				return static_cast<set::binding_type>(r.set_index) == type;
				});

			for (const auto& [name, layout_binding, set_index] : reflected_in_set) {
				auto it = std::ranges::find_if(user_set.bindings, [&](const auto& b) {
					return b.layout_binding.binding == layout_binding.binding;
					});

				assert(
					it != user_set.bindings.end(),
					std::format(
						"Reflected binding '{}' (at binding {}) not found in provided layout for set {}",
						name, 
						layout_binding.binding, 
						static_cast<int>(type)
					)
				);

				assert(it->layout_binding.descriptorType == layout_binding.descriptorType,
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
				{},
				static_cast<uint32_t>(raw.size()),
				raw.data()
			};
			auto raii_layout = device.createDescriptorSetLayout(ci);

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

auto gse::shader::shader_stages() const -> std::array<vk::PipelineShaderStageCreateInfo, 2> {
	return {
		vk::PipelineShaderStageCreateInfo{
			{},
			vk::ShaderStageFlagBits::eVertex,
			*m_vert_module,
			"main"
		},
		vk::PipelineShaderStageCreateInfo{
			{},
			vk::ShaderStageFlagBits::eFragment,
			*m_frag_module,
			"main"
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

auto gse::shader::descriptor_writes(
	vk::DescriptorSet set,
	const std::unordered_map<std::string, vk::DescriptorBufferInfo>& buffer_infos,
	const std::unordered_map<std::string, vk::DescriptorImageInfo>& image_infos
) const -> std::vector<vk::WriteDescriptorSet> {
	std::vector<vk::WriteDescriptorSet> writes;
	std::unordered_set<std::string> used_keys;

	for (const auto& s : m_layout.sets | std::views::values) {
		for (const auto& [binding_name, layout_binding, member] : s.bindings) {
			if (auto it = buffer_infos.find(binding_name); it != buffer_infos.end()) {
				writes.emplace_back(
					set, layout_binding.binding, 0, layout_binding.descriptorCount, layout_binding.descriptorType,
					nullptr, &it->second, nullptr
				);
				used_keys.insert(binding_name);
			}
			else if (auto it2 = image_infos.find(binding_name); it2 != image_infos.end()) {
				writes.emplace_back(
					set, layout_binding.binding, 0, layout_binding.descriptorCount, layout_binding.descriptorType,
					&it2->second, nullptr, nullptr
				);
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
auto gse::shader::set_uniform_block(
	const std::string_view block_name,
	const std::unordered_map<std::string, std::span<const std::byte>>& data,
	const vulkan::persistent_allocator::allocation& alloc
) const -> void {
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
							{},
							layout_binding.binding,
							0,
							1,
							layout_binding.descriptorType,
							&image_info,
						}
					}
				);
				return;
			}
		}
	}
	assert(false, std::format("Binding '{}' not found in shader", name));
}

auto gse::shader::descriptor_set(
	const vk::raii::Device& device,
	const vk::DescriptorPool pool,
	const set::binding_type type,
	const std::uint32_t count
) const -> std::vector<vk::raii::DescriptorSet> {
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
		pool,
		count,
		&set_info
	};

	return device.allocateDescriptorSets(alloc_info);
}

auto gse::shader::descriptor_set(
	const vk::raii::Device& device,
	const vk::DescriptorPool pool,
	const set::binding_type type
) const -> vk::raii::DescriptorSet {
	auto sets = descriptor_set(device, pool, type, 1);
	assert(!sets.empty(), "Failed to allocate descriptor set for shader layout");
	return std::move(sets.front());
}

auto gse::read_file(const std::filesystem::path& path) -> std::vector<char> {
	std::ifstream file(path, std::ios::ate | std::ios::binary);

	assert(file.is_open(), "Failed to open file!");

	const size_t file_size = file.tellg();
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	return buffer;
}