export module gse.vulkan:bindless_mapping;

import std;
import vulkan;

import gse.gpu_backend;

export namespace gse::vulkan {
	struct bindless_mapping_result {
		std::vector<vk::DescriptorSetAndBindingMappingEXT> mappings;
	};

	[[nodiscard]]
	auto build_bindless_mappings(
		std::span<const std::uint32_t> spirv,
		std::uint32_t push_offset_start = 0
	) -> bindless_mapping_result;
}

namespace gse::vulkan {
	struct reflected_binding {
		std::uint32_t set = 0;
		std::uint32_t binding = 0;
		gpu::descriptor_type type = gpu::descriptor_type::storage_buffer;
		bool is_global_params = false;
	};

	[[nodiscard]]
	auto reflect_descriptor_bindings(
		std::span<const std::uint32_t> spirv
	) -> std::vector<reflected_binding>;
}

auto gse::vulkan::reflect_descriptor_bindings(const std::span<const std::uint32_t> spirv) -> std::vector<reflected_binding> {
	std::vector<reflected_binding> result;
	if (spirv.size() < 5 || spirv[0] != 0x07230203u) {
		return result;
	}

	enum class type_kind : std::uint8_t {
		other,
		sampler,
		sampled_image,
		storage_image,
		combined_image_sampler,
		structure,
		acceleration_structure,
	};

	struct pointer_type {
		std::uint32_t storage_class = 0;
		std::uint32_t pointee = 0;
	};

	struct variable {
		std::uint32_t id = 0;
		std::uint32_t pointer_type = 0;
	};

	std::unordered_map<std::uint32_t, std::string> names;
	std::unordered_map<std::uint32_t, std::uint32_t> dec_set;
	std::unordered_map<std::uint32_t, std::uint32_t> dec_binding;
	std::unordered_map<std::uint32_t, pointer_type> pointers;
	std::unordered_map<std::uint32_t, std::uint32_t> array_elements;
	std::unordered_map<std::uint32_t, type_kind> kinds;
	std::vector<variable> variables;

	constexpr std::uint16_t op_name = 5;
	constexpr std::uint16_t op_decorate = 71;
	constexpr std::uint16_t op_type_image = 25;
	constexpr std::uint16_t op_type_sampler = 26;
	constexpr std::uint16_t op_type_sampled_image = 27;
	constexpr std::uint16_t op_type_array = 28;
	constexpr std::uint16_t op_type_runtime_array = 29;
	constexpr std::uint16_t op_type_struct = 30;
	constexpr std::uint16_t op_type_pointer = 32;
	constexpr std::uint16_t op_variable = 59;
	constexpr std::uint16_t op_type_acceleration_structure = 5341;
	constexpr std::uint32_t decoration_binding = 33;
	constexpr std::uint32_t decoration_descriptor_set = 34;
	constexpr std::uint32_t storage_class_uniform = 2;

	std::size_t i = 5;
	while (i < spirv.size()) {
		const std::uint32_t word = spirv[i];
		const auto count = static_cast<std::uint16_t>(word >> 16);
		const auto opcode = static_cast<std::uint16_t>(word & 0xFFFFu);
		if (count == 0 || i + count > spirv.size()) {
			break;
		}

		switch (opcode) {
			case op_name: {
				std::string name;
				bool done = false;
				for (std::size_t w = i + 2; w < i + count && !done; ++w) {
					const auto packed = spirv[w];
					for (int b = 0; b < 4; ++b) {
						const auto c = static_cast<char>((packed >> (b * 8)) & 0xFFu);
						if (c == '\0') {
							done = true;
							break;
						}
						name.push_back(c);
					}
				}
				names[spirv[i + 1]] = std::move(name);
				break;
			}
			case op_decorate: {
				if (count >= 4 && spirv[i + 2] == decoration_descriptor_set) {
					dec_set[spirv[i + 1]] = spirv[i + 3];
				}
				else if (count >= 4 && spirv[i + 2] == decoration_binding) {
					dec_binding[spirv[i + 1]] = spirv[i + 3];
				}
				break;
			}
			case op_type_pointer: {
				pointers[spirv[i + 1]] = {
					.storage_class = spirv[i + 2],
					.pointee = spirv[i + 3],
				};
				break;
			}
			case op_type_array:
			case op_type_runtime_array: {
				array_elements[spirv[i + 1]] = spirv[i + 2];
				break;
			}
			case op_type_image: {
				const auto sampled = count > 7 ? spirv[i + 7] : 0u;
				kinds[spirv[i + 1]] = sampled == 2 ? type_kind::storage_image : type_kind::sampled_image;
				break;
			}
			case op_type_sampler: {
				kinds[spirv[i + 1]] = type_kind::sampler;
				break;
			}
			case op_type_sampled_image: {
				kinds[spirv[i + 1]] = type_kind::combined_image_sampler;
				break;
			}
			case op_type_struct: {
				kinds[spirv[i + 1]] = type_kind::structure;
				break;
			}
			case op_type_acceleration_structure: {
				kinds[spirv[i + 1]] = type_kind::acceleration_structure;
				break;
			}
			case op_variable: {
				variables.push_back({
					.id = spirv[i + 2],
					.pointer_type = spirv[i + 1],
				});
				break;
			}
			default:
				break;
		}
		i += count;
	}

	for (const auto& var : variables) {
		const auto set_it = dec_set.find(var.id);
		const auto binding_it = dec_binding.find(var.id);
		if (set_it == dec_set.end() || binding_it == dec_binding.end()) {
			continue;
		}

		reflected_binding rb{
			.set = set_it->second,
			.binding = binding_it->second,
		};

		if (const auto name_it = names.find(var.id); name_it != names.end() && name_it->second == "globalParams") {
			rb.is_global_params = true;
			rb.type = gpu::descriptor_type::uniform_buffer;
			result.push_back(rb);
			continue;
		}

		const auto ptr_it = pointers.find(var.pointer_type);
		if (ptr_it == pointers.end()) {
			continue;
		}

		std::uint32_t element = ptr_it->second.pointee;
		for (auto arr_it = array_elements.find(element); arr_it != array_elements.end(); arr_it = array_elements.find(element)) {
			element = arr_it->second;
		}

		const auto kind_it = kinds.find(element);
		switch (kind_it != kinds.end() ? kind_it->second : type_kind::other) {
			case type_kind::sampler:
				rb.type = gpu::descriptor_type::sampler;
				break;
			case type_kind::sampled_image:
				rb.type = gpu::descriptor_type::sampled_image;
				break;
			case type_kind::storage_image:
				rb.type = gpu::descriptor_type::storage_image;
				break;
			case type_kind::combined_image_sampler:
				rb.type = gpu::descriptor_type::combined_image_sampler;
				break;
			case type_kind::acceleration_structure:
				rb.type = gpu::descriptor_type::acceleration_structure;
				break;
			case type_kind::structure:
				rb.type = ptr_it->second.storage_class == storage_class_uniform
					? gpu::descriptor_type::uniform_buffer
					: gpu::descriptor_type::storage_buffer;
				break;
			default:
				continue;
		}
		result.push_back(rb);
	}

	return result;
}

auto gse::vulkan::build_bindless_mappings(const std::span<const std::uint32_t> spirv, const std::uint32_t push_offset_start) -> bindless_mapping_result {
	bindless_mapping_result result;

	std::vector<std::uint64_t> seen;
	for (const auto& rb : reflect_descriptor_bindings(spirv)) {
		if (!rb.is_global_params) {
			continue;
		}
		const std::uint64_t key = (static_cast<std::uint64_t>(rb.set) << 24) | (static_cast<std::uint64_t>(rb.binding) << 8) | static_cast<std::uint64_t>(rb.type);
		if (std::ranges::find(seen, key) != seen.end()) {
			continue;
		}
		seen.push_back(key);

		vk::DescriptorMappingSourceDataEXT source{};
		source.pushDataOffset = push_offset_start;
		result.mappings.push_back(vk::DescriptorSetAndBindingMappingEXT{
			.descriptorSet = rb.set,
			.firstBinding = rb.binding,
			.bindingCount = 1,
			.resourceMask = vk::SpirvResourceTypeFlagBitsEXT::eUniformBuffer,
			.source = vk::DescriptorMappingSourceEXT::ePushData,
			.sourceData = source,
		});
	}

	return result;
}
