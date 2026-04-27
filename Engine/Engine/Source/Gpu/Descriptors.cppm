export module gse.gpu:descriptors;

import std;
import vulkan;

import gse.gpu.types;
import gse.gpu.vulkan;
import gse.gpu.context;
import gse.gpu.pipeline;
import gse.gpu.shader;
import gse.assets;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	class descriptor_region final : public non_copyable {
	public:
		descriptor_region() = default;
		descriptor_region(vulkan::descriptor_region&& region);
		descriptor_region(descriptor_region&&) noexcept = default;
		auto operator=(descriptor_region&&) noexcept -> descriptor_region& = default;

		[[nodiscard]] auto native(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_region); }

		operator const vulkan::descriptor_region&(
		) const;

		explicit operator bool() const;
	private:
		vulkan::descriptor_region m_region;
	};

	auto allocate_descriptors(
		context& ctx,
		const shader& s
	) -> descriptor_region;

	class descriptor_writer final : public non_copyable {
	public:
		descriptor_writer(context& ctx, resource::handle<shader> s, descriptor_region& region);
		descriptor_writer(context& ctx, resource::handle<shader> s);
		descriptor_writer(descriptor_writer&&) noexcept = default;
		auto operator=(descriptor_writer&&) noexcept -> descriptor_writer& = default;

		auto buffer(
			std::string_view name,
			const vulkan::buffer_resource& buf
		) -> descriptor_writer&;

		auto buffer(
			std::string_view name,
			const vulkan::buffer_resource& buf,
			std::size_t offset,
			std::size_t range
		) -> descriptor_writer&;

		auto image(
			std::string_view name,
			const vulkan::image_resource& img,
			const sampler& samp,
			image_layout layout = image_layout::shader_read_only
		) -> descriptor_writer&;

		auto image_array(
			std::string_view name,
			std::span<const vulkan::image_resource* const> images,
			const sampler& samp,
			image_layout layout = image_layout::shader_read_only
		) -> descriptor_writer&;

		auto image_array(
			std::string_view name,
			std::span<const vulkan::image_resource* const> images,
			std::span<const sampler* const> samplers,
			image_layout layout = image_layout::shader_read_only
		) -> descriptor_writer&;

		auto storage_image(
			std::string_view name,
			const vulkan::image_resource& img,
			image_layout layout = image_layout::general
		) -> descriptor_writer&;

		auto acceleration_structure(
			std::string_view name,
			acceleration_structure_handle as
		) -> descriptor_writer&;

		auto commit(
		) -> void;

		auto begin(
			std::uint32_t frame_index
		) -> void;

		[[nodiscard]] auto native_writer(this auto&& self) -> auto&& { return std::forward_like<decltype(self)>(self.m_push_writer); }
	private:
		enum class mode { persistent, push };

		auto find_binding_index(
			std::string_view name
		) const -> std::uint32_t;

		const vulkan::shader_cache_entry* m_cache_entry = nullptr;
		vk::raii::Device* m_logical_device = nullptr;
		resource::handle<shader> m_shader;
		descriptor_region* m_region = nullptr;
		mode m_mode = mode::persistent;

		std::unordered_map<std::string, vk::DescriptorBufferInfo> m_buffer_infos;
		std::unordered_map<std::string, vk::DescriptorImageInfo> m_image_infos;
		std::unordered_map<std::string, std::vector<vk::DescriptorImageInfo>> m_image_array_infos;
		std::unordered_map<std::string, vk::DescriptorImageInfo> m_storage_image_infos;
		std::unordered_map<std::string, acceleration_structure_handle> m_as_infos;

		vulkan::descriptor_writer m_push_writer;
	};
}

gse::gpu::descriptor_region::descriptor_region(vulkan::descriptor_region&& region)
	: m_region(region) {}

gse::gpu::descriptor_region::operator const vulkan::descriptor_region&() const {
	return m_region;
}

gse::gpu::descriptor_region::operator bool() const {
	return m_region;
}

auto gse::gpu::allocate_descriptors(context& ctx, const shader& s) -> descriptor_region {
	const auto& cache = ctx.shader_registry().cache(s);
	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	assert(persistent_idx < cache.layout_handles.size(), std::source_location::current(), "Shader has no persistent descriptor set to allocate");

	auto& heap = ctx.descriptor_heap();
	const auto set_layout = cache.layout_handles[persistent_idx];
	const auto size = heap.layout_size(set_layout);
	return descriptor_region(heap.allocate(size));
}

namespace {
	auto build_push_writer(
		gse::gpu::context& ctx,
		const gse::shader& s
	) -> gse::vulkan::descriptor_writer {
		const auto& cache = ctx.shader_registry().cache(s);
		auto& heap = ctx.descriptor_heap();
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
				.type = gse::vulkan::to_vk(b.desc.type)
			};
		}

		return gse::vulkan::descriptor_writer(heap, set_layout, total_size, std::move(bindings));
	}
}

gse::gpu::descriptor_writer::descriptor_writer(context& ctx, resource::handle<shader> s, descriptor_region& region) : m_cache_entry(&ctx.shader_registry().cache(*s)), m_logical_device(&ctx.logical_device()), m_shader(std::move(s)), m_region(&region) {}

gse::gpu::descriptor_writer::descriptor_writer(context& ctx, resource::handle<shader> s) : m_cache_entry(&ctx.shader_registry().cache(*s)), m_logical_device(&ctx.logical_device()), m_shader(std::move(s)), m_mode(mode::push), m_push_writer(build_push_writer(ctx, *m_shader)) {}

auto gse::gpu::descriptor_writer::find_binding_index(const std::string_view name) const -> std::uint32_t {
	for (const auto& [type, bindings] : m_shader->layout_data().sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.name == name) return b.desc.binding;
		}
	}
	assert(false, std::source_location::current(), "Binding '{}' not found in shader", name);
	return 0;
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const vulkan::buffer_resource& buf) -> descriptor_writer& {
	return buffer(name, buf, 0, buf.size);
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const vulkan::buffer_resource& buf, const std::size_t offset, const std::size_t range) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_buffer_infos[std::string(name)] = vk::DescriptorBufferInfo{
			.buffer = buf.buffer,
			.offset = offset,
			.range = range
		};
	}
	else {
		m_push_writer.buffer(find_binding_index(name), buf.buffer, offset, range);
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image(const std::string_view name, const vulkan::image_resource& img, const sampler& samp, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_image_infos[std::string(name)] = vk::DescriptorImageInfo{
			.sampler = samp.native(),
			.imageView = img.view,
			.imageLayout = vulkan::to_vk(layout)
		};
	}
	else {
		m_push_writer.image(find_binding_index(name), img.view, samp.native(), vulkan::to_vk(layout));
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const vulkan::image_resource* const> images, const sampler& samp, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	for (const auto* img : images) {
		vec.push_back({
			.sampler = samp.native(),
			.imageView = img->view,
			.imageLayout = vulkan::to_vk(layout)
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const vulkan::image_resource* const> images, const std::span<const sampler* const> samplers, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	const auto vk_layout = vulkan::to_vk(layout);
	for (std::size_t i = 0; i < images.size(); ++i) {
		vec.push_back({
			.sampler = samplers[i]->native(),
			.imageView = images[i]->view,
			.imageLayout = vk_layout
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::storage_image(const std::string_view name, const vulkan::image_resource& img, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_storage_image_infos[std::string(name)] = vk::DescriptorImageInfo{
			.sampler = nullptr,
			.imageView = img.view,
			.imageLayout = vulkan::to_vk(layout)
		};
	}
	else {
		m_push_writer.storage_image(find_binding_index(name), img.view, vulkan::to_vk(layout));
	}
	return *this;
}

auto gse::gpu::descriptor_writer::acceleration_structure(const std::string_view name, const acceleration_structure_handle as) -> descriptor_writer& {
	m_as_infos[std::string(name)] = as;
	return *this;
}

auto gse::gpu::descriptor_writer::commit() -> void {
	assert(m_region && *m_region, std::source_location::current(), "Cannot commit to null descriptor region");

	const auto& cache = *m_cache_entry;
	const auto& heap = *m_region->native().heap;
	const auto& props = heap.props();

	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	const auto set_layout = cache.layout_handles[persistent_idx];
	const auto& region = m_region->native();

	auto write_binding = [&](const std::string& name, std::uint32_t binding, bool is_uniform, std::uint32_t count) {
		const auto boff = heap.binding_offset(set_layout, binding);

		if (auto it = m_buffer_infos.find(name); it != m_buffer_infos.end()) {
			const auto& [buffer, offset, range] = it->second;
			const auto buf_addr = heap.buffer_address(buffer);

			const vk::DescriptorAddressInfoEXT addr_info{
				.address = buf_addr + offset,
				.range = range,
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
			return;
		}

		if (auto it_arr = m_image_array_infos.find(name); it_arr != m_image_array_infos.end()) {
			const auto& vec = it_arr->second;
			const auto desc_size = props.combined_image_sampler_descriptor_size;

			for (std::size_t i = 0; i < vec.size() && i < count; ++i) {
				const vk::DescriptorGetInfoEXT get_info{
					.type = vk::DescriptorType::eCombinedImageSampler,
					.data = { .pCombinedImageSampler = &vec[i] }
				};
				heap.write_descriptor(region, boff + i * desc_size, get_info, desc_size);
			}
			return;
		}

		if (auto it2 = m_image_infos.find(name); it2 != m_image_infos.end()) {
			const vk::DescriptorGetInfoEXT get_info{
				.type = vk::DescriptorType::eCombinedImageSampler,
				.data = { .pCombinedImageSampler = &it2->second }
			};
			heap.write_descriptor(region, boff, get_info, props.combined_image_sampler_descriptor_size);
			return;
		}

		if (auto it_si = m_storage_image_infos.find(name); it_si != m_storage_image_infos.end()) {
			const vk::DescriptorGetInfoEXT get_info{
				.type = vk::DescriptorType::eStorageImage,
				.data = { .pStorageImage = &it_si->second }
			};
			heap.write_descriptor(region, boff, get_info, props.storage_image_descriptor_size);
			return;
		}

		if (auto it_as = m_as_infos.find(name); it_as != m_as_infos.end()) {
			const auto vk_as = std::bit_cast<vk::AccelerationStructureKHR>(it_as->second.value);
			const vk::DeviceAddress as_addr = m_logical_device->getAccelerationStructureAddressKHR({
				.accelerationStructure = vk_as
			});
			log::println(
				log::category::vulkan,
				"Descriptor AS write: binding='{}' handle={:#x} address={:#x} descriptor_offset={} descriptor_size={}",
				name,
				it_as->second.value,
				as_addr,
				boff,
				props.acceleration_structure_descriptor_size
			);
			if (as_addr == 0) {
				log::println(
					log::level::warning, log::category::vulkan,
					"Descriptor AS write produced address 0 for binding='{}' handle={:#x}",
					name,
					it_as->second.value
				);
			}
			const vk::DescriptorGetInfoEXT get_info{
				.type = vk::DescriptorType::eAccelerationStructureKHR,
				.data = {
					.accelerationStructure = as_addr
				}
			};
			heap.write_descriptor(region, boff, get_info, props.acceleration_structure_descriptor_size);
		}
	};

	if (cache.external_layout) {
		for (const auto& [set_index, bindings] : cache.external_layout->sets()) {
			if (set_index == persistent_idx) {
				for (const auto& [name, layout_binding] : bindings) {
					write_binding(
						name, layout_binding.binding,
						layout_binding.descriptorType == vk::DescriptorType::eUniformBuffer,
						layout_binding.descriptorCount
					);
				}
				break;
			}
		}
	} else {
		const auto& [sets] = m_shader->layout_data();
		const auto set_it = sets.find(descriptor_set_type::persistent);
		if (set_it == sets.end()) return;
		for (const auto& b : set_it->second.bindings) {
			write_binding(b.name, b.desc.binding, b.desc.type == descriptor_type::uniform_buffer, b.desc.count);
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
