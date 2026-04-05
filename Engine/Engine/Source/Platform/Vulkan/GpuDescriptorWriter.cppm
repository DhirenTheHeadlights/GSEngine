export module gse.platform:gpu_descriptor_writer;

import std;

import :gpu_types;
import :gpu_buffer;
import :gpu_pipeline;
import :gpu_image;
import :gpu_descriptor;
import :gpu_device;
import :descriptor_heap;
import :resource_handle;
import :shader;
import gse.assert;
import gse.utility;

export namespace gse::gpu {
	class descriptor_writer final : public non_copyable {
	public:
		descriptor_writer(device& dev, resource::handle<shader> s, descriptor_region& region);
		descriptor_writer(device& dev, resource::handle<shader> s);
		descriptor_writer(descriptor_writer&&) noexcept = default;
		auto operator=(descriptor_writer&&) noexcept -> descriptor_writer& = default;

		auto buffer(std::string_view name, const buffer& buf) -> descriptor_writer&;
		auto buffer(std::string_view name, const gpu::buffer& buf, std::size_t offset, std::size_t range) -> descriptor_writer&;
		auto image(std::string_view name, const image& img, const sampler& samp, image_layout layout = image_layout::shader_read_only) -> descriptor_writer&;
		auto image_array(std::string_view name, std::span<const gpu::image* const> images, const sampler& samp, image_layout layout = image_layout::shader_read_only) -> descriptor_writer&;
		auto image_array(std::string_view name, std::span<const gpu::image* const> images, std::span<const sampler* const> samplers, image_layout layout = image_layout::shader_read_only) -> descriptor_writer&;
		auto acceleration_structure(std::string_view name, acceleration_structure_handle as) -> descriptor_writer&;

		auto commit() -> void;

		auto begin(std::uint32_t frame_index) -> void;

		[[nodiscard]] auto native_writer(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_push_writer); }
	private:
		enum class mode { persistent, push };

		auto find_binding_index(std::string_view name) const -> std::uint32_t;

		device* m_device;
		resource::handle<shader> m_shader;
		descriptor_region* m_region = nullptr;
		mode m_mode = mode::persistent;

		std::unordered_map<std::string, vk::DescriptorBufferInfo> m_buffer_infos;
		std::unordered_map<std::string, vk::DescriptorImageInfo> m_image_infos;
		std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>> m_image_array_infos;
		std::unordered_map<std::string, acceleration_structure_handle> m_as_infos;

		vulkan::descriptor_writer m_push_writer;
	};
}

namespace {
	auto to_vk_layout(const gse::gpu::image_layout l) -> vk::ImageLayout {
		switch (l) {
			case gse::gpu::image_layout::general:          return vk::ImageLayout::eGeneral;
			case gse::gpu::image_layout::shader_read_only: return vk::ImageLayout::eShaderReadOnlyOptimal;
			default:                                       return vk::ImageLayout::eUndefined;
		}
	}

	auto build_push_writer(
		gse::gpu::device& dev,
		const gse::shader& s
	) -> gse::vulkan::descriptor_writer {
		const auto& cache = dev.shader_cache(s);
		auto& heap = dev.descriptor_heap();
		const auto& props = heap.props();

		const auto push_idx = static_cast<std::uint32_t>(gse::gpu::descriptor_set_type::push);
		const auto& layout_data = s.layout_data();

		auto set_it = layout_data.sets.find(gse::gpu::descriptor_set_type::push);
		if (set_it == layout_data.sets.end()) {
			return {};
		}

		const auto& set_data = set_it->second;
		const auto set_layout = cache.layout_handles[push_idx];
		const auto total_size = heap.layout_size(set_layout);

		auto descriptor_size_for = [&](gse::gpu::descriptor_type dt) -> vk::DeviceSize {
			switch (dt) {
				case gse::gpu::descriptor_type::uniform_buffer:          return props.uniform_buffer_descriptor_size;
				case gse::gpu::descriptor_type::storage_buffer:          return props.storage_buffer_descriptor_size;
				case gse::gpu::descriptor_type::combined_image_sampler:  return props.combined_image_sampler_descriptor_size;
				case gse::gpu::descriptor_type::sampled_image:           return props.sampled_image_descriptor_size;
				case gse::gpu::descriptor_type::storage_image:           return props.storage_image_descriptor_size;
				case gse::gpu::descriptor_type::sampler:                 return props.sampler_descriptor_size;
			case gse::gpu::descriptor_type::acceleration_structure:  return props.acceleration_structure_descriptor_size;
			}
			return props.storage_buffer_descriptor_size;
		};

		std::uint32_t max_binding = 0;
		for (const auto& b : set_data.bindings) {
			max_binding = std::max(max_binding, b.desc.binding);
		}

		std::vector<gse::vulkan::descriptor_binding_info> bindings(max_binding + 1);

		for (const auto& b : set_data.bindings) {
			const auto idx = b.desc.binding;
			bindings[idx] = {
				.offset = heap.binding_offset(set_layout, idx),
				.descriptor_size = descriptor_size_for(b.desc.type),
				.type = gse::vulkan::to_vk_descriptor_type(b.desc.type)
			};
		}

		return gse::vulkan::descriptor_writer(heap, set_layout, total_size, std::move(bindings));
	}
}

gse::gpu::descriptor_writer::descriptor_writer(device& dev, resource::handle<shader> s, descriptor_region& region) : m_device(&dev), m_shader(std::move(s)), m_region(&region) {}

gse::gpu::descriptor_writer::descriptor_writer(device& dev, resource::handle<shader> s) : m_device(&dev), m_shader(std::move(s)), m_mode(mode::push), m_push_writer(build_push_writer(dev, *m_shader)) {}

auto gse::gpu::descriptor_writer::find_binding_index(const std::string_view name) const -> std::uint32_t {
	for (const auto& [type, bindings] : m_shader->layout_data().sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.name == name) return b.desc.binding;
		}
	}
	assert(false, std::source_location::current(), "Binding '{}' not found in shader", name);
	return 0;
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const gpu::buffer& buf) -> descriptor_writer& {
	return buffer(name, buf, 0, buf.size());
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const gpu::buffer& buf, const std::size_t offset, const std::size_t range) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_buffer_infos[std::string(name)] = vk::DescriptorBufferInfo{
			.buffer = buf.native().buffer,
			.offset = offset,
			.range = range
		};
	}
	else {
		m_push_writer.buffer(find_binding_index(name), buf, offset, range);
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image(const std::string_view name, const gpu::image& img, const sampler& samp, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_image_infos[std::string(name)] = vk::DescriptorImageInfo{
			.sampler = samp.native(),
			.imageView = img.native().view,
			.imageLayout = to_vk_layout(layout)
		};
	}
	else {
		m_push_writer.image(find_binding_index(name), img, samp, layout);
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const gpu::image* const> images, const sampler& samp, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	for (const auto* img : images) {
		vec.push_back({
			.sampler = samp.native(),
			.imageView = img->native().view,
			.imageLayout = to_vk_layout(layout)
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::acceleration_structure(const std::string_view name, const acceleration_structure_handle as) -> descriptor_writer& {
	m_as_infos[std::string(name)] = as;
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const gpu::image* const> images, const std::span<const sampler* const> samplers, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	const auto vk_layout = to_vk_layout(layout);
	for (std::size_t i = 0; i < images.size(); ++i) {
		vec.push_back({
			.sampler = samplers[i]->native(),
			.imageView = images[i]->native().view,
			.imageLayout = vk_layout
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::commit() -> void {
	assert(m_region && *m_region, std::source_location::current(), "Cannot commit to null descriptor region");

	const auto& cache = m_device->shader_cache(*m_shader);
	auto& heap = *m_region->native().heap;
	const auto& props = heap.props();

	const auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	const auto set_layout = cache.layout_handles[persistent_idx];

	const auto& layout_data = m_shader->layout_data();
	auto set_it = layout_data.sets.find(descriptor_set_type::persistent);
	if (set_it == layout_data.sets.end()) return;

	const auto& region = m_region->native();

	for (const auto& b : set_it->second.bindings) {
		const auto boff = heap.binding_offset(set_layout, b.desc.binding);

		if (auto it = m_buffer_infos.find(b.name); it != m_buffer_infos.end()) {
			const auto& buf_info = it->second;
			const auto buf_addr = heap.buffer_address(buf_info.buffer);

			const bool is_uniform = b.desc.type == descriptor_type::uniform_buffer;

			const vk::DescriptorAddressInfoEXT addr_info{
				.address = buf_addr + buf_info.offset,
				.range = buf_info.range,
				.format = vk::Format::eUndefined
			};

			if (is_uniform) {
				const vk::DescriptorGetInfoEXT get_info{
					.type = vk::DescriptorType::eUniformBuffer,
					.data = { .pUniformBuffer = &addr_info }
				};
				heap.write_descriptor(region, boff, get_info, props.uniform_buffer_descriptor_size);
			} else {
				const vk::DescriptorGetInfoEXT get_info{
					.type = vk::DescriptorType::eStorageBuffer,
					.data = { .pStorageBuffer = &addr_info }
				};
				heap.write_descriptor(region, boff, get_info, props.storage_buffer_descriptor_size);
			}
			continue;
		}

		if (auto it_arr = m_image_array_infos.find(b.name); it_arr != m_image_array_infos.end()) {
			const auto& vec = it_arr->second;
			const auto desc_size = props.combined_image_sampler_descriptor_size;

			for (std::size_t i = 0; i < vec.size() && i < b.desc.count; ++i) {
				const vk::DescriptorGetInfoEXT get_info{
					.type = vk::DescriptorType::eCombinedImageSampler,
					.data = { .pCombinedImageSampler = &vec[i] }
				};
				heap.write_descriptor(region, boff + i * desc_size, get_info, desc_size);
			}
			continue;
		}

		if (auto it2 = m_image_infos.find(b.name); it2 != m_image_infos.end()) {
			const vk::DescriptorGetInfoEXT get_info{
				.type = vk::DescriptorType::eCombinedImageSampler,
				.data = { .pCombinedImageSampler = &it2->second }
			};
			heap.write_descriptor(region, boff, get_info, props.combined_image_sampler_descriptor_size);
			continue;
		}

		if (auto it_as = m_as_infos.find(b.name); it_as != m_as_infos.end()) {
			const auto vk_as = reinterpret_cast<VkAccelerationStructureKHR>(it_as->second.value);
			const vk::DeviceAddress as_addr = m_device->logical_device().getAccelerationStructureAddressKHR({ .accelerationStructure = vk_as });
			const vk::DescriptorGetInfoEXT get_info{
				.type = vk::DescriptorType::eAccelerationStructureKHR,
				.data = { .accelerationStructure = as_addr }
			};
			heap.write_descriptor(region, boff, get_info, props.acceleration_structure_descriptor_size);
		}
	}

	m_buffer_infos.clear();
	m_image_infos.clear();
	m_image_array_infos.clear();
	m_as_infos.clear();
}

auto gse::gpu::descriptor_writer::begin(const std::uint32_t frame_index) -> void {
	m_push_writer.begin(frame_index);
}
