export module gse.gpu:descriptors;

import std;

import :types;
import :vulkan_device;
import :vulkan_acceleration_structure;
import :descriptor_heap;
import :shader;
import :shader_registry;
import gse.assets;

import gse.assert;
import gse.log;
import gse.core;
import gse.containers;
import gse.time;
import gse.concurrency;
import gse.diag;

export namespace gse::gpu {
	auto allocate_descriptors(
		shader_registry& registry,
		descriptor_heap& heap,
		const resource::handle<shader>& s
	) -> descriptor_region;

	class descriptor_writer final : public non_copyable {
	public:
		descriptor_writer(
			shader_registry& registry,
			handle<device> dev,
			resource::handle<shader> s,
			descriptor_region& region
		);
		descriptor_writer(
			shader_registry& registry,
			handle<device> dev,
			descriptor_heap& heap,
			resource::handle<shader> s
		);
		descriptor_writer(descriptor_writer&&) noexcept = default;
		auto operator=(descriptor_writer&&) noexcept -> descriptor_writer& = default;

		auto buffer(
			std::string_view name,
			const vulkan::basic_buffer<vulkan::device>& buf
		) -> descriptor_writer&;

		auto buffer(
			std::string_view name,
			const vulkan::basic_buffer<vulkan::device>& buf,
			std::size_t offset,
			std::size_t range
		) -> descriptor_writer&;

		auto image(
			std::string_view name,
			const vulkan::basic_image<vulkan::device>& img,
			const sampler& samp,
			image_layout layout = image_layout::shader_read_only
		) -> descriptor_writer&;

		auto image_array(
			std::string_view name,
			std::span<const vulkan::basic_image<vulkan::device>* const> images,
			const sampler& samp,
			image_layout layout = image_layout::shader_read_only
		) -> descriptor_writer&;

		auto image_array(
			std::string_view name,
			std::span<const vulkan::basic_image<vulkan::device>* const> images,
			std::span<const sampler* const> samplers,
			image_layout layout = image_layout::shader_read_only
		) -> descriptor_writer&;

		auto storage_image(
			std::string_view name,
			const vulkan::basic_image<vulkan::device>& img,
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

		struct stored_buffer_info {
			handle<buffer> buf;
			std::size_t offset = 0;
			std::size_t range = 0;
		};

		const shader_cache_entry* m_cache_entry = nullptr;
		handle<device> m_device;
		resource::handle<shader> m_shader;
		descriptor_region* m_region = nullptr;
		mode m_mode = mode::persistent;

		std::unordered_map<std::string, stored_buffer_info> m_buffer_infos;
		std::unordered_map<std::string, descriptor_image_info> m_image_infos;
		std::unordered_map<std::string, std::vector<descriptor_image_info>> m_image_array_infos;
		std::unordered_map<std::string, descriptor_image_info> m_storage_image_infos;
		std::unordered_map<std::string, acceleration_structure_handle> m_as_infos;

		descriptor_set_writer m_push_writer;
	};
}

auto gse::gpu::allocate_descriptors(shader_registry& registry, descriptor_heap& heap, const resource::handle<shader>& s) -> descriptor_region {
	const auto& cache = registry.cache(s);
	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	assert(persistent_idx < cache.layout_handles.size(), std::source_location::current(), "Shader has no persistent descriptor set to allocate");

	const auto set_layout = cache.layout_handles[persistent_idx];
	const auto size = heap.layout_size(set_layout);
	return heap.allocate(size);
}

namespace {
	auto build_push_writer(
		gse::gpu::shader_registry& registry,
		gse::gpu::descriptor_heap& heap,
		const gse::resource::handle<gse::shader>& s
	) -> gse::gpu::descriptor_set_writer {
		const auto& cache = registry.cache(s);
		const auto& props = heap.props();

		const auto push_idx = static_cast<std::uint32_t>(gse::gpu::descriptor_set_type::push);
		const auto& layout_data = s->layout_data();

		auto set_it = layout_data.sets.find(gse::gpu::descriptor_set_type::push);
		if (set_it == layout_data.sets.end()) {
			return {};
		}

		const auto& set_data = set_it->second;
		const auto set_layout = cache.layout_handles[push_idx];
		const auto total_size = heap.layout_size(set_layout);

		auto descriptor_size_for = [&](gse::gpu::descriptor_type dt) -> gse::gpu::device_size {
			switch (dt) {
				case gse::gpu::descriptor_type::uniform_buffer:
					return props.uniform_buffer_descriptor_size;
				case gse::gpu::descriptor_type::storage_buffer:
					return props.storage_buffer_descriptor_size;
				case gse::gpu::descriptor_type::combined_image_sampler:
					return props.combined_image_sampler_descriptor_size;
				case gse::gpu::descriptor_type::sampled_image:
					return props.sampled_image_descriptor_size;
				case gse::gpu::descriptor_type::storage_image:
					return props.storage_image_descriptor_size;
				case gse::gpu::descriptor_type::sampler:
					return props.sampler_descriptor_size;
				case gse::gpu::descriptor_type::acceleration_structure:
					return props.acceleration_structure_descriptor_size;
			}
			return props.storage_buffer_descriptor_size;
		};

		std::uint32_t max_binding = 0;
		for (const auto& b : set_data.bindings) {
			max_binding = std::max(max_binding, b.desc.binding);
		}

		std::vector<gse::gpu::descriptor_binding_info> bindings(max_binding + 1);

		for (const auto& b : set_data.bindings) {
			const auto idx = b.desc.binding;
			bindings[idx] = {
				.offset = heap.binding_offset(set_layout, idx),
				.descriptor_size = descriptor_size_for(b.desc.type),
				.type = b.desc.type,
			};
		}

		return gse::gpu::descriptor_set_writer(heap, set_layout, total_size, std::move(bindings));
	}
}

gse::gpu::descriptor_writer::descriptor_writer(shader_registry& registry, const handle<device> dev, resource::handle<shader> s, descriptor_region& region) : m_cache_entry(&registry.cache(s)), m_device(dev), m_shader(std::move(s)), m_region(&region) {}

gse::gpu::descriptor_writer::descriptor_writer(shader_registry& registry, const handle<device> dev, descriptor_heap& heap, resource::handle<shader> s) : m_cache_entry(&registry.cache(s)), m_device(dev), m_shader(std::move(s)), m_mode(mode::push), m_push_writer(build_push_writer(registry, heap, m_shader)) {}

auto gse::gpu::descriptor_writer::find_binding_index(const std::string_view name) const -> std::uint32_t {
	for (const auto& [type, bindings] : m_shader->layout_data().sets | std::views::values) {
		for (const auto& b : bindings) {
			if (b.name == name) return b.desc.binding;
		}
	}
	assert(false, std::source_location::current(), "Binding '{}' not found in shader", name);
	return 0;
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const vulkan::basic_buffer<vulkan::device>& buf) -> descriptor_writer& {
	return buffer(name, buf, 0, buf.size_bytes());
}

auto gse::gpu::descriptor_writer::buffer(const std::string_view name, const vulkan::basic_buffer<vulkan::device>& buf, const std::size_t offset, const std::size_t range) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_buffer_infos[std::string(name)] = stored_buffer_info{
			.buf = buf.handle(),
			.offset = offset,
			.range = range,
		};
	}
	else {
		m_push_writer.buffer(find_binding_index(name), buf.handle(), offset, range);
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image(const std::string_view name, const vulkan::basic_image<vulkan::device>& img, const sampler& samp, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_image_infos[std::string(name)] = descriptor_image_info{
			.sampler = samp.native(),
			.image_view = img.view(),
			.layout = layout,
		};
	}
	else {
		m_push_writer.image(find_binding_index(name), img.view(), samp.native(), layout);
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const vulkan::basic_image<vulkan::device>* const> images, const sampler& samp, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	for (const auto* img : images) {
		vec.push_back({
			.sampler = samp.native(),
			.image_view = img->view(),
			.layout = layout,
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::image_array(const std::string_view name, const std::span<const vulkan::basic_image<vulkan::device>* const> images, const std::span<const sampler* const> samplers, const image_layout layout) -> descriptor_writer& {
	auto& vec = m_image_array_infos[std::string(name)];
	vec.clear();
	vec.reserve(images.size());
	for (std::size_t i = 0; i < images.size(); ++i) {
		vec.push_back({
			.sampler = samplers[i]->native(),
			.image_view = images[i]->view(),
			.layout = layout,
		});
	}
	return *this;
}

auto gse::gpu::descriptor_writer::storage_image(const std::string_view name, const vulkan::basic_image<vulkan::device>& img, const image_layout layout) -> descriptor_writer& {
	if (m_mode == mode::persistent) {
		m_storage_image_infos[std::string(name)] = descriptor_image_info{
			.sampler = 0,
			.image_view = img.view(),
			.layout = layout,
		};
	}
	else {
		m_push_writer.storage_image(find_binding_index(name), img.view(), layout);
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
	const auto& heap = *m_region->heap;
	const auto& props = heap.props();

	constexpr auto persistent_idx = static_cast<std::uint32_t>(descriptor_set_type::persistent);
	const auto set_layout = cache.layout_handles[persistent_idx];
	const auto& region = *m_region;

	auto write_binding = [&](const std::string& name, std::uint32_t binding, bool is_uniform, std::uint32_t count) {
		const auto boff = heap.binding_offset(set_layout, binding);

		if (auto it = m_buffer_infos.find(name); it != m_buffer_infos.end()) {
			const auto& [buf, offset, range] = it->second;
			const auto buf_addr = heap.buffer_address(buf);

			const descriptor_get_info get_info{
				.type = is_uniform ? descriptor_type::uniform_buffer : descriptor_type::storage_buffer,
				.buffer = {
					.address = buf_addr + offset,
					.range = range,
				},
			};
			const auto size = is_uniform ? props.uniform_buffer_descriptor_size : props.storage_buffer_descriptor_size;
			heap.write_descriptor(region, boff, get_info, size);
			return;
		}

		if (auto it_arr = m_image_array_infos.find(name); it_arr != m_image_array_infos.end()) {
			const auto& vec = it_arr->second;
			const auto desc_size = props.combined_image_sampler_descriptor_size;

			for (std::size_t i = 0; i < vec.size() && i < count; ++i) {
				const descriptor_get_info get_info{
					.type = descriptor_type::combined_image_sampler,
					.image = vec[i],
				};
				heap.write_descriptor(region, boff + i * desc_size, get_info, desc_size);
			}
			return;
		}

		if (auto it2 = m_image_infos.find(name); it2 != m_image_infos.end()) {
			const descriptor_get_info get_info{
				.type = descriptor_type::combined_image_sampler,
				.image = it2->second,
			};
			heap.write_descriptor(region, boff, get_info, props.combined_image_sampler_descriptor_size);
			return;
		}

		if (auto it_si = m_storage_image_infos.find(name); it_si != m_storage_image_infos.end()) {
			const descriptor_get_info get_info{
				.type = descriptor_type::storage_image,
				.image = it_si->second,
			};
			heap.write_descriptor(region, boff, get_info, props.storage_image_descriptor_size);
			return;
		}

		if (auto it_as = m_as_infos.find(name); it_as != m_as_infos.end()) {
			const auto as_addr = vulkan::acceleration_structure_address_from_handle(m_device, it_as->second);
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
			const descriptor_get_info get_info{
				.type = descriptor_type::acceleration_structure,
				.acceleration_structure = as_addr,
			};
			heap.write_descriptor(region, boff, get_info, props.acceleration_structure_descriptor_size);
		}
	};

	if (cache.external_layout) {
		for (const auto& [set_index, bindings] : cache.external_layout->sets()) {
			if (set_index == persistent_idx) {
				for (const auto& b : bindings) {
					write_binding(
						b.name,
						b.desc.binding,
						b.desc.type == descriptor_type::uniform_buffer,
						b.desc.count
					);
				}
				break;
			}
		}
	}
	else {
		const auto& [sets] = m_shader->layout_data();
		const auto set_it = sets.find(descriptor_set_type::persistent);
		if (set_it == sets.end()) {
			return;
		}
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
