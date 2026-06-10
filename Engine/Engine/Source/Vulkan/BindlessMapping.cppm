export module gse.vulkan:bindless_mapping;

import std;
import vulkan;

import gse.gpu_backend;

export namespace gse::vulkan {
	struct bindless_mapping_result {
		std::vector<vk::DescriptorSetAndBindingMappingEXT> mappings;
		std::uint32_t push_data_size = 0;
	};

	[[nodiscard]]
	auto build_bindless_mappings(
		std::span<const gpu::binding_use> bindings,
		const gpu::bindless_layout& layout,
		std::uint32_t push_offset_start = 0
	) -> bindless_mapping_result;
}

auto gse::vulkan::build_bindless_mappings(const std::span<const gpu::binding_use> bindings, const gpu::bindless_layout& layout, const std::uint32_t push_offset_start) -> bindless_mapping_result {
	bindless_mapping_result result;
	result.mappings.reserve(bindings.size());

	std::vector<gpu::binding_use> sorted_bindings(bindings.begin(), bindings.end());
	std::ranges::sort(
		sorted_bindings,
		[](const gpu::binding_use& a, const gpu::binding_use& b) {
			if (a.set != b.set) {
				return a.set < b.set;
			}
			return a.slot < b.slot;
		}
	);

	const auto image_offset = static_cast<std::uint32_t>(layout.image_range_offset);
	const auto image_stride = static_cast<std::uint32_t>(layout.image_stride);
	const auto texture_image_offset = static_cast<std::uint32_t>(layout.texture_image_offset);
	const auto texture_sampler_offset = static_cast<std::uint32_t>(layout.texture_sampler_offset);
	const auto buffer_offset = static_cast<std::uint32_t>(layout.buffer_range_offset);
	const auto buffer_stride = static_cast<std::uint32_t>(layout.buffer_stride);
	const auto sampler_offset = static_cast<std::uint32_t>(layout.sampler_range_offset);
	const auto sampler_stride = static_cast<std::uint32_t>(layout.sampler_stride);

	std::uint32_t push_offset = push_offset_start;
	for (const auto& b : sorted_bindings) {
		const bool is_array = b.count > 1;

		vk::DescriptorMappingSourceDataEXT source_data{};
		vk::SpirvResourceTypeFlagsEXT resource_mask{};

		if (is_array) {
			auto& co = source_data.constantOffset;
			const auto write_resource_fields = [&](const std::uint32_t heap_off, const std::uint32_t stride) {
				co.heapOffset = heap_off;
				co.heapArrayStride = stride;
			};
			const auto write_sampler_fields = [&](const std::uint32_t heap_off, const std::uint32_t stride) {
				co.samplerHeapOffset = heap_off;
				co.samplerHeapArrayStride = stride;
			};

			switch (b.type) {
				case gpu::descriptor_type::sampled_image:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eSampledImage;
					write_resource_fields(image_offset, image_stride);
					break;
				case gpu::descriptor_type::storage_image:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eReadOnlyImage | vk::SpirvResourceTypeFlagBitsEXT::eReadWriteImage;
					write_resource_fields(image_offset, image_stride);
					break;
				case gpu::descriptor_type::combined_image_sampler:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eCombinedSampledImage;
					write_resource_fields(texture_image_offset, image_stride);
					write_sampler_fields(texture_sampler_offset, sampler_stride);
					break;
				case gpu::descriptor_type::sampler:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eSampler;
					write_sampler_fields(sampler_offset, sampler_stride);
					break;
				case gpu::descriptor_type::uniform_buffer:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eUniformBuffer;
					write_resource_fields(buffer_offset, buffer_stride);
					break;
				case gpu::descriptor_type::storage_buffer:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eReadOnlyStorageBuffer | vk::SpirvResourceTypeFlagBitsEXT::eReadWriteStorageBuffer;
					write_resource_fields(buffer_offset, buffer_stride);
					break;
				case gpu::descriptor_type::acceleration_structure:
					resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eAccelerationStructure;
					write_resource_fields(buffer_offset, buffer_stride);
					break;
			}

			result.mappings.push_back(vk::DescriptorSetAndBindingMappingEXT{
				.descriptorSet = b.set,
				.firstBinding = b.slot,
				.bindingCount = 1,
				.resourceMask = resource_mask,
				.source = vk::DescriptorMappingSourceEXT::eHeapWithConstantOffset,
				.sourceData = source_data,
			});
			continue;
		}

		auto& pi = source_data.pushIndex;

		const auto write_resource_fields = [&](const std::uint32_t heap_off, const std::uint32_t stride) {
			pi.heapOffset = heap_off;
			pi.pushOffset = push_offset;
			pi.heapIndexStride = stride;
			pi.heapArrayStride = 0;
			push_offset += static_cast<std::uint32_t>(sizeof(std::uint32_t));
		};
		const auto write_sampler_fields = [&]() {
			pi.samplerHeapOffset = sampler_offset;
			pi.samplerPushOffset = push_offset;
			pi.samplerHeapIndexStride = sampler_stride;
			pi.samplerHeapArrayStride = 0;
			push_offset += static_cast<std::uint32_t>(sizeof(std::uint32_t));
		};

		switch (b.type) {
			case gpu::descriptor_type::sampled_image:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eSampledImage;
				write_resource_fields(image_offset, image_stride);
				break;
			case gpu::descriptor_type::storage_image:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eReadOnlyImage | vk::SpirvResourceTypeFlagBitsEXT::eReadWriteImage;
				write_resource_fields(image_offset, image_stride);
				break;
			case gpu::descriptor_type::combined_image_sampler:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eCombinedSampledImage;
				pi.useCombinedImageSamplerIndex = vk::False;
				write_resource_fields(image_offset, image_stride);
				write_sampler_fields();
				break;
			case gpu::descriptor_type::sampler:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eSampler;
				write_sampler_fields();
				break;
			case gpu::descriptor_type::uniform_buffer:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eUniformBuffer;
				write_resource_fields(buffer_offset, buffer_stride);
				break;
			case gpu::descriptor_type::storage_buffer:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eReadOnlyStorageBuffer | vk::SpirvResourceTypeFlagBitsEXT::eReadWriteStorageBuffer;
				write_resource_fields(buffer_offset, buffer_stride);
				break;
			case gpu::descriptor_type::acceleration_structure:
				resource_mask = vk::SpirvResourceTypeFlagBitsEXT::eAccelerationStructure;
				write_resource_fields(buffer_offset, buffer_stride);
				break;
		}

		result.mappings.push_back(vk::DescriptorSetAndBindingMappingEXT{
			.descriptorSet = b.set,
			.firstBinding = b.slot,
			.bindingCount = 1,
			.resourceMask = resource_mask,
			.source = vk::DescriptorMappingSourceEXT::eHeapWithPushIndex,
			.sourceData = source_data,
		});
	}

	result.push_data_size = push_offset;

	return result;
}
